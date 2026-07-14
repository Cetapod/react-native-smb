#include "ContextRequestQueue.hpp"

#include "PoolContextHandle.hpp"
#include "../core/SmbEnums.hpp"

namespace react_native_smb {

size_t ContextRequestQueue::pendingCount() const {
    return interactivePending_.size() + metadataPending_.size();
}

void ContextRequestQueue::drainCancelledFromHead(std::deque<ContextRequestPtr>& queue) {
    while (!queue.empty()) {
        auto& front = queue.front();
        if (!front->cancelled.load(std::memory_order_acquire)) break;
        if (!front->fulfilled.load(std::memory_order_acquire)) {
            front->fulfilled.store(true, std::memory_order_release);
            try {
                front->promise.set_value(PoolContextHandle{});
            } catch (...) {
            }
        }
        queue.pop_front();
    }
}

ContextRequestPtr ContextRequestQueue::enqueue(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId) {
    auto req = std::make_shared<ContextRequest>();
    req->id = nextId_++;
    req->mode = mode;
    req->kind = kind;
    req->taskId = taskId;

    auto& queue = (mode == AcquireMode::Interactive) ? interactivePending_ : metadataPending_;
    queue.push_back(req);
    return req;
}

ContextRequestPtr ContextRequestQueue::takeNext() {
    drainCancelledFromHead(interactivePending_);
    if (!interactivePending_.empty()) {
        ContextRequestPtr out = std::move(interactivePending_.front());
        interactivePending_.pop_front();
        return out;
    }

    drainCancelledFromHead(metadataPending_);
    if (!metadataPending_.empty()) {
        ContextRequestPtr out = std::move(metadataPending_.front());
        metadataPending_.pop_front();
        return out;
    }

    return nullptr;
}

void ContextRequestQueue::requeueFront(ContextRequestPtr req) {
    if (!req) return;
    auto& queue = (req->mode == AcquireMode::Interactive) ? interactivePending_ : metadataPending_;
    queue.push_front(std::move(req));
}

void ContextRequestQueue::cancelForTask(const std::string& taskId) {
    auto cancelIn = [&](std::deque<ContextRequestPtr>& queue) {
        for (auto& req : queue) {
            if (req->taskId != taskId) continue;
            if (req->fulfilled.load(std::memory_order_acquire)) continue;
            req->cancelled.store(true, std::memory_order_release);
            req->fulfilled.store(true, std::memory_order_release);
            try {
                req->promise.set_value(PoolContextHandle{});
            } catch (...) {
            }
        }
    };
    cancelIn(interactivePending_);
    cancelIn(metadataPending_);
}

void ContextRequestQueue::removeCancelledFromHead() {
    drainCancelledFromHead(interactivePending_);
    drainCancelledFromHead(metadataPending_);
}

}  // namespace react_native_smb