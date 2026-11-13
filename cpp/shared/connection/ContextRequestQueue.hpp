#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "ContextRequest.hpp"

namespace react_native_smb {

// Interactive-first FIFO of pending context requests. Not thread-safe; the pool guards it.
class ContextRequestQueue {
   public:
    void enqueue(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId, ContextCallbacks callbacks);
    ContextRequestPtr takeNext();
    void requeueFront(ContextRequestPtr req);
    // Remove pending requests so the caller can reject them outside the lock.
    std::vector<ContextRequestPtr> takeForTask(const std::string& taskId);
    std::vector<ContextRequestPtr> takeAll();

   private:
    std::deque<ContextRequestPtr> interactivePending_;
    std::deque<ContextRequestPtr> metadataPending_;
    uint64_t nextId_{1};
};

}  // namespace react_native_smb
