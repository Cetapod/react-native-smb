#pragma once

#include "../SmbFileIO.hpp"

namespace react_native_smb {

// Internal variant that reports when the server atomically creates the destination.
int64_t smbCopyFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& fromPath,
                         const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                         std::function<void(double, double)> progressHandler, const CancellationToken& cancel,
                         std::function<void()> onDestinationClaimed);

}  // namespace react_native_smb
