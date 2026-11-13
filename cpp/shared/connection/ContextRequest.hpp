#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "PoolTypes.hpp"
#include "SlotReservation.hpp"

namespace react_native_smb {

struct ContextRequest {
    uint64_t id{0};
    AcquireMode mode{AcquireMode::Interactive};
    SmbOperatorKind kind{SmbOperatorKind::Initialize};
    std::string taskId;
    ContextCallbacks callbacks;
    // Set while the slot activates; the reservation then fails instead of assigning.
    std::atomic<bool> cancelled{false};
};

using ContextRequestPtr = std::shared_ptr<ContextRequest>;

}  // namespace react_native_smb
