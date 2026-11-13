#pragma once

#include <string>

#include "../../core/SmbModels.hpp"
#include "../OperatorBase.hpp"

namespace react_native_smb {

class InitializeOperator : public OperatorBase {
   public:
    InitializeOperator(std::string url, SmbCredentials credentials) : url_(std::move(url)), credentials_(std::move(credentials)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::Initialize; }
    std::optional<AcquireMode> acquireMode() const override { return std::nullopt; }
    void run() override;

   private:
    std::string url_;
    SmbCredentials credentials_;
};

}  // namespace react_native_smb
