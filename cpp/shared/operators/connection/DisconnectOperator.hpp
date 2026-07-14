#pragma once

#include "../OperatorBase.hpp"

namespace react_native_smb {

class DisconnectOperator : public OperatorBase {
   public:
    DisconnectOperator() = default;

    SmbOperatorKind kind() const override { return SmbOperatorKind::Disconnect; }
    void run() override;
};

}  // namespace react_native_smb
