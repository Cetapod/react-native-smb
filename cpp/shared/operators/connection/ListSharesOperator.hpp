#pragma once

#include <vector>

#include "../../core/SmbModels.hpp"
#include "../Operator.hpp"

namespace react_native_smb {

class ListSharesOperator : public Operator<std::vector<SmbShare>> {
   public:
    ListSharesOperator() = default;

    SmbOperatorKind kind() const override { return SmbOperatorKind::ListShares; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Interactive; }
    void run() override;
};

}  // namespace react_native_smb
