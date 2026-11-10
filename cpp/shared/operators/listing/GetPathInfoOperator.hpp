#pragma once

#include <string>

#include "../../core/SmbModels.hpp"
#include "../Operator.hpp"

namespace react_native_smb {

class GetPathInfoOperator : public Operator<SmbFileInfo> {
   public:
    explicit GetPathInfoOperator(std::string path) : path_(std::move(path)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::GetPathInfo; }
    void run() override;

   private:
    std::string path_;
};

}  // namespace react_native_smb
