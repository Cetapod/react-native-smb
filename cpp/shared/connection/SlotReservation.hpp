#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "PoolContextHandle.hpp"

namespace react_native_smb {

class SmbConnectionPool;
struct ContextRequest;

// A pool slot reserved for one request and marked Activating. The holder calls
// activate() on its own thread, so connecting a fresh slot never runs on the
// thread that queued or released work. Dropping it unactivated frees the slot.
class SlotReservation {
   public:
    SlotReservation(SmbConnectionPool* pool, size_t slotIndex, std::shared_ptr<ContextRequest> request);
    ~SlotReservation();

    SlotReservation(SlotReservation&& o) noexcept;
    SlotReservation& operator=(SlotReservation&&) = delete;
    SlotReservation(const SlotReservation&) = delete;
    SlotReservation& operator=(const SlotReservation&) = delete;

    // Connects the slot if needed and assigns it. Throws when activation fails,
    // the request was cancelled, or the pool is closing.
    PoolContextHandle activate();

   private:
    SmbConnectionPool* pool_{nullptr};
    size_t slotIndex_{0};
    std::shared_ptr<ContextRequest> request_;
};

// Exactly one callback runs per request, never under the pool lock.
struct ContextCallbacks {
    std::function<void(SlotReservation)> onGranted;
    std::function<void(int code, const std::string& message)> onRejected;
};

}  // namespace react_native_smb
