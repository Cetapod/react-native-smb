#pragma once

#include "../connection/PoolTypes.hpp"
#include "../core/SmbEnums.hpp"
#include "../core/SmbModels.hpp"
#include "../core/SmbTypes.hpp"
#include "PoolInfo.hpp"
#include "PoolSlotInfo.hpp"
#include "SmbCredentials.hpp"
#include "SmbOperatorKind.hpp"
#include "SmbTaskState.hpp"
#include "SlotState.hpp"
#include "TaskStatus.hpp"

#include <vector>

namespace margelo::nitro::smb {

inline react_native_smb::SmbCredentials fromBridge(const SmbCredentials& credentials) {
    return react_native_smb::SmbCredentials{credentials.username, credentials.password};
}

inline SmbOperatorKind toBridge(react_native_smb::SmbOperatorKind kind) {
    return static_cast<SmbOperatorKind>(static_cast<int>(kind));
}

inline TaskStatus toBridge(react_native_smb::SmbTaskStatus status) {
    return static_cast<TaskStatus>(static_cast<int>(status));
}

inline SlotState toBridge(react_native_smb::SlotState state) {
    return static_cast<SlotState>(static_cast<int>(state));
}

inline SmbTaskState toBridge(const react_native_smb::SmbTaskState& state) {
    return SmbTaskState(
        state.taskId,
        toBridge(state.kind),
        toBridge(state.status),
        react_native_smb::isDeterminate(state.kind),
        state.progress,
        static_cast<double>(state.bytesDone),
        static_cast<double>(state.totalBytes),
        state.bytesPerSecond,
        state.etaSeconds,
        state.sourcePath,
        state.destinationPath,
        static_cast<double>(state.errorCode),
        state.errorMessage,
        static_cast<double>(state.startedAt),
        static_cast<double>(state.updatedAt),
        static_cast<double>(state.endedAt));
}

inline PoolSlotInfo toBridge(const react_native_smb::PoolSlotInfo& slot) {
    return PoolSlotInfo(
        static_cast<double>(slot.index),
        slot.interactiveOnly,
        toBridge(slot.state),
        slot.isConnected,
        slot.shareName,
        slot.taskId,
        toBridge(slot.kind));
}

inline PoolInfo toBridge(const std::vector<react_native_smb::PoolSlotInfo>& status) {
    std::vector<PoolSlotInfo> slots;
    slots.reserve(status.size());
    double poolSize = 0;
    for (const auto& slot : status) {
        poolSize = static_cast<double>(slot.poolSize);
        slots.push_back(toBridge(slot));
    }
    return PoolInfo(poolSize, std::move(slots));
}

}  // namespace margelo::nitro::smb
