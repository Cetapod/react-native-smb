#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

class CreateDirectoryOperator : public OperatorBase {
   public:
    explicit CreateDirectoryOperator(std::string path) : path_(std::move(path)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::CreateDirectory; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Interactive; }
    void run() override;

   private:
    std::string path_;
};

}  // namespace react_native_smb
