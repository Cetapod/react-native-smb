#pragma once

#include "../core/SmbModels.hpp"
#include "SmbCredentials.hpp"

namespace margelo::nitro::smb {

inline react_native_smb::SmbCredentials fromBridge(const SmbCredentials& credentials) {
    return react_native_smb::SmbCredentials{credentials.username, credentials.password};
}

}  // namespace margelo::nitro::smb
