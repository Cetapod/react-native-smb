#include "SmbTask.hpp"

#include <optional>

#include "../connection/SmbConnectionPool.hpp"
#include "../operators/OperatorBase.hpp"
#include "../util/SmbErrorMapper.hpp"
#include "../util/SmbException.hpp"
#include "EntryStream.hpp"
namespace react_native_smb {

SmbTask::SmbTask(std::string id, std::unique_ptr<OperatorBase> op, std::shared_ptr<SmbConnectionPool> pool)
    : SmbTaskCore(std::move(id), op->kind()), op_(std::move(op)), pool_(pool) {
    if (op_->kind() == SmbOperatorKind::ListDirectory) {
        entries_ = std::make_shared<EntryStream>();
    }
}

SmbTask::~SmbTask() = default;

void SmbTask::cancel() {
    if (isSettled()) return;
    // Trip the shared flag first so in-flight poll loops see cancel immediately.
    SmbTaskCore::cancel();
    if (auto pool = pool_.lock()) pool->cancelRequestsForTask(getId());
}

void SmbTask::run() {
    markRunning();
    auto pool = pool_.lock();
    if (!pool) {
        if (!cancel_.cancelled()) {
            settle(SmbTaskStatus::Error, "SMB connection pool is unavailable", static_cast<int>(SmbErrorCode::NotConnected));
        }
    } else if (!cancel_.cancelled()) {
        try {
            op_->attach(this, pool.get(), cancel_);
            op_->prepare();
            std::optional<PoolContextHandle> context;
            if (const auto mode = op_->acquireMode()) {
                markPending();
                context = pool->requestContext(*mode, getKind(), getId());
                markRunning();
            }
            op_->start(context ? &*context : nullptr);
        } catch (const SmbException& e) {
            if (e.code() == SmbErrorCode::Cancelled) {
                SmbTaskCore::cancel();
            } else {
                settle(SmbTaskStatus::Error, e.what(), e.codeInt());
            }
        } catch (const std::exception& e) {
            const int code = SmbErrorMapper::fromErrnoOrMessage(0, e.what());
            settle(SmbTaskStatus::Error, e.what(), code);
        } catch (...) {
            settle(SmbTaskStatus::Error, "unknown error", static_cast<int>(SmbErrorCode::Unknown));
        }
    }
    maybeSettle();
}

}  // namespace react_native_smb
