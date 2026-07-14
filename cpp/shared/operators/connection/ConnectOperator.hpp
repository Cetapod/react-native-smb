#pragma once

#include <string>

#include "../../ReactNativeSmb.hpp"  // SmbCredentials, SmbConnectionInfo
#include "../Operator.hpp"

namespace react_native_smb {

class ConnectOperator : public Operator<SmbConnectionInfo> {
   public:
    ConnectOperator(std::string url, SmbCredentials credentials) : url_(std::move(url)), credentials_(std::move(credentials)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::Connect; }
    void run() override;

   private:
    std::string url_;
    SmbCredentials credentials_;
};

}  // namespace react_native_smb
