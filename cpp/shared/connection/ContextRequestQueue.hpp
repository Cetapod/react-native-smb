#pragma once

#include <cstdint>
#include <deque>
#include <memory>

#include "ContextRequest.hpp"

namespace react_native_smb {

class ContextRequestQueue {
   public:
    ContextRequestPtr enqueue(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId);
    ContextRequestPtr takeNext();
    void requeueFront(ContextRequestPtr req);
    void removeCancelledFromHead();
    void cancelForTask(const std::string& taskId);
    void cancelAll();

   private:
    size_t pendingCount() const;
    void drainCancelledFromHead(std::deque<ContextRequestPtr>& queue);

    std::deque<ContextRequestPtr> interactivePending_;
    std::deque<ContextRequestPtr> metadataPending_;
    uint64_t nextId_{1};
};

}  // namespace react_native_smb
