#include "PoolSlot.hpp"

namespace react_native_smb {

PoolSlot::PoolSlot(size_t index) : index_(index), conn_(std::make_unique<SmbConnectionManager>()) {}

bool PoolSlot::accepts(AcquireMode mode) const {
    if (index_ == 0) return mode == AcquireMode::Interactive;
    return true;
}

PoolSlotInfo PoolSlot::snapshot(size_t poolSize) const {
    PoolSlotInfo info;
    info.poolSize = poolSize;
    info.index = index_;
    info.interactiveOnly = isInteractiveOnly();
    info.state = state();
    info.isConnected = conn_ && conn_->isConnected();
    info.shareName = conn_ ? conn_->getShareName() : std::string();
    info.taskId = activeTaskId_;
    info.kind = isIdle() ? SmbOperatorKind::Initialize : activeKind_;
    return info;
}

void PoolSlot::assign(uint64_t requestId, const std::string& taskId, SmbOperatorKind kind) {
    assignedRequestId_ = requestId;
    activeTaskId_ = taskId;
    activeKind_ = kind;
    state_ = SlotState::Assigned;
}

void PoolSlot::release() {
    assignedRequestId_ = 0;
    activeTaskId_.clear();
    activeKind_ = SmbOperatorKind::Initialize;
    state_ = SlotState::Idle;
}

void PoolSlot::markActivating() { state_ = SlotState::Activating; }

void PoolSlot::activate(const PoolConnectParams& params, const std::string& taskId) {
    state_ = SlotState::Activating;
    if (!params.serverUrl.empty() && params.credentials) {
        conn_->initialize(params.serverUrl, *params.credentials, taskId);
    }
    if (!params.shareName.empty()) {
        conn_->connectShare(params.shareName, taskId);
    }
}

void PoolSlot::ensureShare(const std::string& share, const std::string& taskId) {
    if (share.empty()) {
        return;
    }
    if (conn_->getShareName() != share) {
        conn_->connectShare(share, taskId);
    } else if (!conn_->isConnected()) {
        conn_->checkAndConnect(taskId);
    }
}

void PoolSlot::resetBroken() {
    release();
    if (conn_) {
        try {
            conn_->disconnect();
        } catch (...) {
        }
    }
    conn_ = std::make_unique<SmbConnectionManager>();
}

}  // namespace react_native_smb