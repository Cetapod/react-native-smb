#include "SmbTask.hpp"

#include <thread>

#include "../connection/SmbConnectionPool.hpp"
#include "../operators/OperatorBase.hpp"
#include "../util/SmbErrorMapper.hpp"
#include "../util/SmbException.hpp"
#include "EntryStream.hpp"
namespace react_native_smb {

SmbTask::SmbTask(std::string id, std::unique_ptr<OperatorBase> seedOp, std::shared_ptr<SmbConnectionPool> pool)
    : SmbTaskCore(std::move(id), seedOp->kind()), pool_(pool) {
    if (seedOp->kind() == SmbOperatorKind::ListDirectory) {
        entries_ = std::make_shared<EntryStream>();
    }
    ops_.push_back(std::move(seedOp));
    opProgress_.resize(1);
}

SmbTask::~SmbTask() = default;

void SmbTask::cancel() {
    if (isSettled()) return;
    // Trip the shared flag first so in-flight poll loops see cancel immediately.
    SmbTaskCore::cancel();
    if (auto pool = pool_.lock()) pool->cancelRequestsForTask(getId());
}

void SmbTask::start() {
    markRunning();
    auto fut = completionFuture();
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        pending_ = 1;
        launchOp(0);
    }
    fut.wait();
}

void SmbTask::launchOp(size_t opIndex) {
    auto self = shared_from_this();
    std::thread([self, opIndex] {
        if (self->cancel_.cancelled()) {
            self->onOpFinished();
            return;
        }
        OperatorBase* op = nullptr;
        {
            std::lock_guard<std::mutex> lk(self->stateMutex_);
            if (opIndex < self->ops_.size()) op = self->ops_[opIndex].get();
        }
        if (!op) {
            self->onOpFinished();
            return;
        }
        auto pool = self->pool_.lock();
        if (!pool) {
            if (!self->cancel_.cancelled()) {
                self->settle(SmbTaskStatus::Error, "SMB connection pool is unavailable", static_cast<int>(SmbErrorCode::NotConnected));
            }
            self->onOpFinished();
            return;
        }
        try {
            op->start(self.get(), pool.get(), self->cancel_, opIndex);
        } catch (const SmbException& e) {
            if (e.code() == SmbErrorCode::Cancelled) {
                self->SmbTaskCore::cancel();
            } else {
                self->settle(SmbTaskStatus::Error, e.what(), e.codeInt());
            }
        } catch (const std::exception& e) {
            const int code = SmbErrorMapper::fromErrnoOrMessage(0, e.what());
            self->settle(SmbTaskStatus::Error, e.what(), code);
        } catch (...) {
            self->settle(SmbTaskStatus::Error, "unknown error", static_cast<int>(SmbErrorCode::Unknown));
        }
        self->onOpFinished();
    }).detach();
}

size_t SmbTask::appendOps(std::vector<std::unique_ptr<OperatorBase>>& more) {
    const size_t first = ops_.size();
    for (auto& op : more) {
        ops_.push_back(std::move(op));
    }
    opProgress_.resize(ops_.size());
    return first;
}

size_t SmbTask::spawn(std::vector<std::unique_ptr<OperatorBase>> moreOps) {
    if (moreOps.empty()) return ops_.size();
    const size_t count = moreOps.size();
    size_t first;
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        first = appendOps(moreOps);
        pending_ += count;
        for (size_t i = 0; i < count; ++i) launchOp(first + i);
    }
    return first;
}

}  // namespace react_native_smb
