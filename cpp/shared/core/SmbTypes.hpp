#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "SmbEnums.hpp"

namespace react_native_smb {

// Immutable snapshot a task broadcasts to its subscribers. Pure data (DTO).
// Projected to/from a Nitro string map by TaskSnapshotCodec.
struct SmbTaskState {
    std::string taskId;
    SmbOperatorKind kind{SmbOperatorKind::Initialize};
    SmbTaskStatus status{SmbTaskStatus::Idle};
    double progress{0.0};  // 0.0 - 1.0 (meaningful only when isDeterminate(kind))
    int errorCode{0};
    std::string errorMessage;
    std::string sourcePath;
    std::string destinationPath;
    int64_t bytesDone{0};
    int64_t totalBytes{0};
    double bytesPerSecond{0.0};
    double etaSeconds{0.0};
    int64_t startedAt{0};
    int64_t updatedAt{0};
    int64_t endedAt{0};  // 0 if not settled
};

// Any subscriber (JS bridge, observer hub, anyone). The task notifies all
// subscribers without knowing who they are.
using SnapshotListener = std::function<void(const SmbTaskState&)>;

}  // namespace react_native_smb
