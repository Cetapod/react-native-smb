#pragma once

#include <string>

#include "../../ReactNativeSmb.hpp"  // SmbConnectionInfo
#include "../Operator.hpp"

namespace react_native_smb {

class ConnectShareOperator : public Operator<SmbConnectionInfo> {
   public:
    explicit ConnectShareOperator(std::string share) : share_(std::move(share)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::ConnectShare; }
    void run() override;

   private:
    std::string share_;
};

}  // namespace react_native_smb
