#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

class MoveItemOperator : public OperatorBase {
   public:
    MoveItemOperator(std::string fromPath, std::string toPath) : fromPath_(std::move(fromPath)), toPath_(std::move(toPath)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::MoveItem; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Interactive; }
    void prepare() override;
    void run() override;

   private:
    std::string fromPath_;
    std::string toPath_;
    std::string from_;
    std::string to_;
};

}  // namespace react_native_smb
