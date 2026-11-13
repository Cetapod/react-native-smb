#include "SlotReservation.hpp"

#include <utility>

#include "SmbConnectionPool.hpp"

namespace react_native_smb {

SlotReservation::SlotReservation(SmbConnectionPool* pool, size_t slotIndex, std::shared_ptr<ContextRequest> request)
    : pool_(pool), slotIndex_(slotIndex), request_(std::move(request)) {}

SlotReservation::~SlotReservation() {
    if (pool_) pool_->abandonReservation(slotIndex_, request_);
}

SlotReservation::SlotReservation(SlotReservation&& o) noexcept
    : pool_(std::exchange(o.pool_, nullptr)), slotIndex_(o.slotIndex_), request_(std::move(o.request_)) {}

PoolContextHandle SlotReservation::activate() {
    return std::exchange(pool_, nullptr)->activateReservation(slotIndex_, request_);
}

}  // namespace react_native_smb
