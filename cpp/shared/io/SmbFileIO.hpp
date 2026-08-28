// SMB file I/O helpers — sync and async pipelined variants.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "../ReactNativeSmb.hpp"
#include "../core/CancellationToken.hpp"

namespace react_native_smb {

class SmbConnectionManager;

// Sync helpers run on the I/O thread.

// Read remote SMB file to local path. Returns total bytes read.
int64_t smbReadFile(
    void* ctx, const std::string& remotePath, const std::string& localPath, std::function<void(double, double)> progressHandler = [](double, double) {}, const CancellationToken& cancel = {});

// Upload local file to remote SMB path. Returns total bytes written.
int64_t smbWriteFile(
    void* ctx, const std::string& localPath, const std::string& remotePath, std::function<void(double, double)> progressHandler = [](double, double) {}, const CancellationToken& cancel = {});

// Copy SMB file to another SMB location. Returns total bytes copied.
// totalBytesCopied: shared counter for cumulative progress across multiple files.
int64_t smbCopyFile(void* ctx, const std::string& fromPath, const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                    std::function<void(double, double)> progressHandler = {}, const CancellationToken& cancel = {});

// Async pipelined helpers (drive poll loop on caller thread; call under connectionMutex_).

// Async pipelined download. Returns total bytes read.
int64_t smbReadFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& remotePath, const std::string& localPath, std::function<void(double, double)> progressHandler = {}, const CancellationToken& cancel = {});

// Async pipelined upload. Returns total bytes written.
int64_t smbWriteFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& localPath, const std::string& remotePath, std::function<void(double, double)> progressHandler = {}, const CancellationToken& cancel = {});

// Async pipelined SMB->SMB copy on the same context.
// totalBytesCopied: shared counter for cumulative progress. Returns bytes copied.
int64_t smbCopyFileAsync(void* ctx, SmbConnectionManager& manager, const std::string& fromPath, const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                         std::function<void(double, double)> progressHandler = {}, const CancellationToken& cancel = {});

}  // namespace react_native_smb
