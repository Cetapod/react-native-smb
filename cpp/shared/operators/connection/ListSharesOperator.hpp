#pragma once

#include <vector>

#include "../../core/SmbModels.hpp"
#include "../Operator.hpp"

namespace react_native_smb {

class ListSharesOperator : public Operator<std::vector<SmbShare>> {
   public:
    ListSharesOperator() = default;

    SmbOperatorKind kind() const override { return SmbOperatorKind::ListShares; }
    void run() override;
};

}  // namespace react_native_smb
