#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "SmbTransferCore.hpp"

struct smb2fh;

namespace react_native_smb {

class SmbConnectionManager;

namespace detail {

enum class ServerCopyResult {
    Copied,       // every byte was copied by the server (or core.failure is set)
    Unsupported,  // rejected before any data was copied; both handles are untouched
};

// Copies core.fileSize bytes from `source` to `destination` inside the server
// (FSCTL_SRV_REQUEST_RESUME_KEY + FSCTL_SRV_COPYCHUNK_WRITE), so the data never
// crosses the network. Progress goes through core like the client-side copy.
// Failures after data started moving are recorded in core.failure.
ServerCopyResult serverSideCopy(TransferCore& core, SmbConnectionManager& manager, smb2fh* source, smb2fh* destination,
                                const std::shared_ptr<int64_t>& totalCopied, int64_t totalSize,
                                const std::function<void()>& clearHandles);

}  // namespace detail
}  // namespace react_native_smb
