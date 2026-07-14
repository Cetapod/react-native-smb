#pragma once

#include <atomic>
#include <memory>

namespace react_native_smb {

// A cheap, copyable cancellation flag shared by a task and all its operators.
// The task owns the root token; cancel() trips it; operators poll cancelled()
// inside their work loops. Copying shares the same underlying atomic<bool>.
class CancellationToken {
   public:
    CancellationToken() = default;

    void cancel() { flag_->store(true, std::memory_order_relaxed); }
    bool cancelled() const { return flag_->load(std::memory_order_relaxed); }

   private:
    std::shared_ptr<std::atomic<bool>> flag_ = std::make_shared<std::atomic<bool>>(false);
};

}  // namespace react_native_smb
