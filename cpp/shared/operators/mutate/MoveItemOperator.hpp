#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

class MoveItemOperator : public OperatorBase {
   public:
    MoveItemOperator(std::string fromPath, std::string toPath) : fromPath_(std::move(fromPath)), toPath_(std::move(toPath)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::MoveItem; }
    void run() override;

   private:
    std::string fromPath_;
    std::string toPath_;
};

}  // namespace react_native_smb
