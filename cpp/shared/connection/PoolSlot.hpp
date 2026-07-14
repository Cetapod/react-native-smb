#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "PoolTypes.hpp"
#include "SmbConnection.hpp"

namespace react_native_smb {

class PoolSlot {
   public:
    explicit PoolSlot(size_t index);

    size_t index() const { return index_; }
    SlotState state() const { return state_; }

    bool isInteractiveOnly() const { return index_ == 0; }
    bool isIdle() const { return state() == SlotState::Idle; }
    bool accepts(AcquireMode mode) const;

    SmbConnectionManager* manager() { return conn_.get(); }
    const SmbConnectionManager* manager() const { return conn_.get(); }
    smb2_context* rawCtx() const { return conn_ ? conn_->rawCtx() : nullptr; }

    PoolSlotInfo snapshot(size_t poolSize) const;

    void assign(uint64_t requestId, const std::string& taskId, SmbOperatorKind kind);
    void release();
    void markActivating();
    void activate(const PoolConnectParams& params, const std::string& taskId);
    void ensureShare(const std::string& share, const std::string& taskId);
    void resetBroken();

   private:
    size_t index_{0};
    SlotState state_{SlotState::Idle};
    std::unique_ptr<SmbConnectionManager> conn_;
    std::string activeTaskId_;
    SmbOperatorKind activeKind_{SmbOperatorKind::Initialize};
    uint64_t assignedRequestId_{0};
};

}  // namespace react_native_smb