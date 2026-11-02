#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../core/CancellationToken.hpp"

struct smb2_context;

namespace react_native_smb {

class SmbConnectionManager;

namespace detail {

struct FileCopyJob {
    std::string from;  // normalized
    std::string to;    // normalized
};

// Copies files on a held context with server-side copy, up to `maxFiles` at
// once; each file is open → resume key → COPYCHUNK_WRITE → fsync → close, as
// separate pipelined requests. Use only when the server is known to support
// server-side copy: a rejection fails the copy. On failure or cancel, files in
// progress are closed and their partial destinations removed; finished files
// stay. Returns normally when cancelled; throws SmbException on failure.
void copyFilesServerSide(smb2_context* ctx, SmbConnectionManager& manager, const std::vector<FileCopyJob>& files, size_t maxFiles,
                         const std::shared_ptr<int64_t>& totalCopied, int64_t totalSize,
                         const std::function<void(double, double)>& onProgress, const CancellationToken& cancel);

}  // namespace detail
}  // namespace react_native_smb
