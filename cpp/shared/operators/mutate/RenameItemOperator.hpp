#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

class RenameItemOperator : public OperatorBase {
   public:
    RenameItemOperator(std::string currentPath, std::string newName) : currentPath_(std::move(currentPath)), newName_(std::move(newName)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::RenameItem; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Interactive; }
    void prepare() override;
    void run() override;

   private:
    std::string currentPath_;
    std::string newName_;
    std::string from_;
    std::string to_;
};

}  // namespace react_native_smb
