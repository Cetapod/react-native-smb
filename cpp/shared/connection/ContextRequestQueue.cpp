#include "ContextRequestQueue.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace react_native_smb {

void ContextRequestQueue::enqueue(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId, ContextCallbacks callbacks) {
    auto req = std::make_shared<ContextRequest>();
    req->id = nextId_++;
    req->mode = mode;
    req->kind = kind;
    req->taskId = taskId;
    req->callbacks = std::move(callbacks);

    auto& queue = (mode == AcquireMode::Interactive) ? interactivePending_ : metadataPending_;
    queue.push_back(std::move(req));
}

ContextRequestPtr ContextRequestQueue::takeNext() {
    for (auto* queue : {&interactivePending_, &metadataPending_}) {
        if (queue->empty()) continue;
        ContextRequestPtr out = std::move(queue->front());
        queue->pop_front();
        return out;
    }
    return nullptr;
}

void ContextRequestQueue::requeueFront(ContextRequestPtr req) {
    if (!req) return;
    auto& queue = (req->mode == AcquireMode::Interactive) ? interactivePending_ : metadataPending_;
    queue.push_front(std::move(req));
}

std::vector<ContextRequestPtr> ContextRequestQueue::takeForTask(const std::string& taskId) {
    std::vector<ContextRequestPtr> out;
    for (auto* queue : {&interactivePending_, &metadataPending_}) {
        auto matches = [&](const ContextRequestPtr& req) { return req->taskId == taskId; };
        std::copy_if(queue->begin(), queue->end(), std::back_inserter(out), matches);
        queue->erase(std::remove_if(queue->begin(), queue->end(), matches), queue->end());
    }
    return out;
}

std::vector<ContextRequestPtr> ContextRequestQueue::takeAll() {
    std::vector<ContextRequestPtr> out(interactivePending_.begin(), interactivePending_.end());
    out.insert(out.end(), metadataPending_.begin(), metadataPending_.end());
    interactivePending_.clear();
    metadataPending_.clear();
    return out;
}

}  // namespace react_native_smb
