#pragma once

#include <string>
#include <variant>
#include <vector>

#include "SmbModels.hpp"

namespace react_native_smb {

// Sentinel for listDirectory: entries live in the task's EntryStream.
struct ListingResult {};

// Typed result storage for domain tasks. Bridge converts to JSI.
using SmbTaskResult = std::variant<std::monostate, std::string, SmbConnectionInfo, std::vector<SmbShare>, SmbFileInfo,
                                   SmbSecurityDescriptor, ListingResult>;

}  // namespace react_native_smb
