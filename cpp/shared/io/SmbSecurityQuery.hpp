#pragma once

#include <string>
#include <stdexcept>

#include "../ReactNativeSmb.hpp"

struct smb2_context;

namespace react_native_smb {

class SmbConnectionManager;

class SmbSecurityQueryTransportError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Query owner, group, and DACL for one path using an already-acquired context.
SmbSecurityDescriptor querySecurityDescriptorOnCtx(smb2_context* ctx, const std::string& path, SmbConnectionManager& manager);

}  // namespace react_native_smb
