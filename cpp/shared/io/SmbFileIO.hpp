// SMB file I/O helpers - async pipelined variants.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "../core/CancellationToken.hpp"

namespace react_native_smb {

class SmbConnectionManager;

// Async pipelined helpers (drive poll loop on caller thread; call under connectionMutex_).

// Async pipelined download. Writes to a temp sibling, then renames to localPath on success.
// taskId is used to form the unique temp filename.
int64_t smbReadFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& remotePath,
                         const std::string& localPath, const std::string& taskId,
                         std::function<void(double, double)> progressHandler = {},
                         const CancellationToken& cancel = {});

// Async pipelined upload. Writes to a remote temp sibling, then renames to remotePath on success.
int64_t smbWriteFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& localPath,
                          const std::string& remotePath, const std::string& taskId,
                          std::function<void(double, double)> progressHandler = {},
                          const CancellationToken& cancel = {});

// Async pipelined SMB->SMB copy on the same context.
// totalBytesCopied: shared counter for cumulative progress. Returns bytes copied.
int64_t smbCopyFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& fromPath,
                         const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                         std::function<void(double, double)> progressHandler = {}, const CancellationToken& cancel = {});

}  // namespace react_native_smb
