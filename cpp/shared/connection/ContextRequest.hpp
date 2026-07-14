#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

#include "PoolTypes.hpp"

namespace react_native_smb {

class PoolContextHandle;

struct ContextRequest {
    uint64_t id{0};
    AcquireMode mode{AcquireMode::Interactive};
    SmbOperatorKind kind{SmbOperatorKind::Initialize};
    std::string taskId;
    std::promise<PoolContextHandle> promise;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> fulfilled{false};
};

using ContextRequestPtr = std::shared_ptr<ContextRequest>;

}  // namespace react_native_smb