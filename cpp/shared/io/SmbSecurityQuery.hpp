#pragma once

#include <string>

#include "../ReactNativeSmb.hpp"

struct smb2_context;

namespace react_native_smb {

class SmbConnectionManager;

// Query owner, group, and DACL for one path using an already-acquired context.
SmbSecurityDescriptor querySecurityDescriptorOnCtx(smb2_context* ctx, const std::string& path, SmbConnectionManager& manager);

}  // namespace react_native_smb
