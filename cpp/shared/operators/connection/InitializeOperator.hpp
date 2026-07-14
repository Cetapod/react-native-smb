#pragma once

#include <string>

#include "../../ReactNativeSmb.hpp"  // SmbCredentials
#include "../OperatorBase.hpp"

namespace react_native_smb {

class InitializeOperator : public OperatorBase {
   public:
    InitializeOperator(std::string url, SmbCredentials credentials) : url_(std::move(url)), credentials_(std::move(credentials)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::Initialize; }
    void run() override;

   private:
    std::string url_;
    SmbCredentials credentials_;
};

}  // namespace react_native_smb
