#pragma once

#include <string>

#include "../../core/SmbModels.hpp"
#include "../Operator.hpp"

namespace react_native_smb {

// Queries a path security descriptor via CREATE + QUERY_INFO + CLOSE.
class GetSecurityDescriptorOperator : public Operator<SmbSecurityDescriptor> {
   public:
    explicit GetSecurityDescriptorOperator(std::string path) : path_(std::move(path)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::GetSecurityDescriptor; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Interactive; }
    void run() override;

   private:
    std::string path_;
};

}  // namespace react_native_smb
