#pragma once

#include <atomic>

namespace react_native_smb {

// What the connected server and share are known to support. One instance is
// shared by every pool slot and replaced whenever the pool switches server or
// share, so a capability is probed once per pool rather than once per slot.
struct ServerCapabilities {
    // Set once the server rejects server-side copy; later copies skip the attempt.
    std::atomic<bool> serverSideCopyUnsupported{false};
};

}  // namespace react_native_smb
