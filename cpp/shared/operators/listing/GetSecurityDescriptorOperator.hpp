#pragma once

#include <string>

#include "../../ReactNativeSmb.hpp"  // SmbSecurityDescriptor
#include "../Operator.hpp"

namespace react_native_smb {

// Queries a path security descriptor via CREATE + QUERY_INFO + CLOSE.
class GetSecurityDescriptorOperator : public Operator<SmbSecurityDescriptor> {
   public:
    explicit GetSecurityDescriptorOperator(std::string path) : path_(std::move(path)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::GetSecurityDescriptor; }
    void run() override;

   private:
    std::string path_;
};

}  // namespace react_native_smb
