#include "SmbTask.hpp"

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

bool SmbTask::prepare() {
    auto pool = pool_.lock();
    if (!pool) {
        settle(SmbTaskStatus::Error, "SMB connection pool is unavailable", static_cast<int>(SmbErrorCode::NotConnected));
        return false;
    }
    op_->attach(this, pool.get(), cancel_);
    try {
        op_->prepare();
        return true;
    } catch (...) {
        settleFromCurrentException();
    }
    maybeSettle();
    return false;
}

std::optional<AcquireMode> SmbTask::acquireMode() const { return op_->acquireMode(); }

void SmbTask::run(std::optional<SlotReservation> reservation) {
    if (!cancel_.cancelled()) {
        try {
            std::optional<PoolContextHandle> context;
            if (reservation) context = reservation->activate();
            markRunning();
            op_->start(context ? &*context : nullptr);
        } catch (...) {
            settleFromCurrentException();
        }
    }
    maybeSettle();
}

void SmbTask::reject(int code, const std::string& message) {
    if (code == static_cast<int>(SmbErrorCode::Cancelled)) {
        SmbTaskCore::cancel();
    } else {
        settle(SmbTaskStatus::Error, message, code);
    }
    maybeSettle();
}

void SmbTask::settleFromCurrentException() {
    try {
        throw;
    } catch (const SmbException& e) {
        if (e.code() == SmbErrorCode::Cancelled) {
            SmbTaskCore::cancel();
        } else {
            settle(SmbTaskStatus::Error, e.what(), e.codeInt());
        }
    } catch (const std::exception& e) {
        settle(SmbTaskStatus::Error, e.what(), SmbErrorMapper::fromErrnoOrMessage(0, e.what()));
    } catch (...) {
        settle(SmbTaskStatus::Error, "unknown error", static_cast<int>(SmbErrorCode::Unknown));
    }
}

}  // namespace react_native_smb
