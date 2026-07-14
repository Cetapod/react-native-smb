#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "../core/SmbEnums.hpp"

namespace react_native_smb {

struct SmbCredentials;

enum class AcquireMode {
    Interactive,
    Metadata,
};

enum class SlotState {
    Idle,
    Assigned,
    Activating,
};

struct PoolConnectParams {
    std::string serverUrl;
    std::string shareName;
    std::shared_ptr<SmbCredentials> credentials;
};

struct PoolSlotInfo {
    size_t poolSize{0};
    size_t index{0};
    bool interactiveOnly{false};
    SlotState state{SlotState::Idle};
    bool isConnected{false};
    std::string shareName;
    std::string taskId;
    SmbOperatorKind kind{SmbOperatorKind::Initialize};
};

}  // namespace react_native_smb