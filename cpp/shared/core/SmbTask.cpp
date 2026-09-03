#include "SmbTask.hpp"

#include <NitroModules/Promise.hpp>
#include <thread>

#include "../connection/SmbConnectionPool.hpp"
#include "../operators/OperatorBase.hpp"
#include "../util/SmbErrorMapper.hpp"
#include "../util/SmbException.hpp"
#include "TaskSnapshotCodec.hpp"
#include "../util/SmbLog.hpp"

namespace react_native_smb {

namespace jsi = facebook::jsi;
using namespace margelo::nitro;

SmbTask::SmbTask(std::string id, std::unique_ptr<OperatorBase> seedOp, std::shared_ptr<SmbConnectionPool> pool)
    : HybridObject("SmbTask"),
      SmbTaskCore(std::move(id), seedOp->kind()),
      pool_(pool) {
    ops_.push_back(std::move(seedOp));
    opProgress_.resize(1);
    loadHybridMethods();
}

SmbTask::~SmbTask() = default;

void SmbTask::cancel() {
    if (isSettled()) return;
    // Trip the shared flag first so in-flight poll loops see cancel immediately.
    // Pool queue cancellation can wake a waiting op with SmbException::Cancelled
    // on the worker thread; the flag must already be set to avoid settling Error.
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
    auto self = std::static_pointer_cast<SmbTask>(shared());
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
            if (e.code() == SmbErrorCode::Cancelled || self->cancel_.cancelled()) {
                self->SmbTaskCore::cancel();
            } else {
                self->settle(SmbTaskStatus::Error, e.what(), e.codeInt());
            }
        } catch (const std::exception& e) {
            if (self->cancel_.cancelled()) {
                self->SmbTaskCore::cancel();
            } else {
                const int code = SmbErrorMapper::fromErrnoOrMessage(0, e.what());
                self->settle(SmbTaskStatus::Error, e.what(), code);
            }
        } catch (...) {
            if (self->cancel_.cancelled()) {
                self->SmbTaskCore::cancel();
            } else {
                self->settle(SmbTaskStatus::Error, "unknown error", static_cast<int>(SmbErrorCode::Unknown));
            }
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

std::unordered_map<std::string, std::string> SmbTask::get() {
    return TaskSnapshotCodec::encode(getState());
}

jsi::Value SmbTask::getResultValueRaw(jsi::Runtime& runtime, const jsi::Value& /*thisValue*/, const jsi::Value* /*args*/,
                                      size_t /*count*/) {
    return SmbTaskCore::getResultValue(runtime);
}

std::string SmbTask::subscribeMap(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) {
    return SmbTaskCore::subscribe([listener](const SmbTaskState& s) {
        if (listener) {
            try {
                listener(TaskSnapshotCodec::encode(s));
            } catch (...) {
            }
        }
    });
}

void SmbTask::loadHybridMethods() {
    HybridObject::loadHybridMethods();
    registerHybrids(this, [](Prototype& prototype) {
        prototype.registerHybridMethod("id", &SmbTask::getId);
        prototype.registerHybridMethod("cancel", &SmbTask::cancel);
        prototype.registerHybridMethod("get", &SmbTask::get);
        prototype.registerHybridMethod("subscribe", &SmbTask::subscribeMap);
        prototype.registerHybridMethod("unsubscribe", &SmbTask::unsubscribe);
        prototype.registerRawHybridMethod("getResultValue", 0, &SmbTask::getResultValueRaw);
    });
}

}  // namespace react_native_smb
