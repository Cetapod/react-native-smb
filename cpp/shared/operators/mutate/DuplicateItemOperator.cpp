#include "DuplicateItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <string>

#include "../../io/SmbCopyTree.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../io/SmbTreeOps.hpp"
#include "../../util/SmbException.hpp"

namespace react_native_smb {

void DuplicateItemOperator::run() {
    const CancellationToken token = cancelToken();

    const std::string sourcePath = path_util::normalized(path_);
    const std::string parentPath = path_util::parentOf(sourcePath);
    const std::string fileName = path_util::extractFileName(sourcePath);
    setSourceDestination(sourcePath, sourcePath);

    MetadataContextLease lease = makeMetadataLease();
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    const CopyTreeSourceInfo source = inspectCopyTreeSource(lease, sourcePath, /*recursive=*/true, token);
    constexpr int kMaxRootClaimRetries = 64;

    std::string destPath;
    bool rootClaimed = false;
    int nextNameIndex = 1;
    for (int attempt = 0; attempt < kMaxRootClaimRetries; ++attempt) {
        if (isCancelled()) {
            emitStatus(SmbTaskStatus::Cancelled);
            return;
        }

        lease.submitSync([&](smb2_context* ctx) {
            const std::string newName =
                tree_ops::generateUniqueCopyName(ctx, parentPath, fileName, token, nextNameIndex);
            destPath = path_util::buildUrl(parentPath, newName);
        });

        try {
            copyTreeImpl(
                pool(), kind(), sourcePath, destPath, /*recursive=*/true, token,
                [this](double done, double total) { emitProgress(done, total); },
                [this](int64_t n) { addExpectedBytes(n); }, owningTaskId(), &lease, &source,
                [&] {
                    rootClaimed = true;
                    setSourceDestination(sourcePath, destPath);
                });
            break;
        } catch (const SmbException& error) {
            if (rootClaimed || error.code() != SmbErrorCode::AlreadyExists) {
                throw;
            }
            if (isCancelled()) {
                emitStatus(SmbTaskStatus::Cancelled);
                return;
            }
            if (attempt + 1 == kMaxRootClaimRetries) {
                SmbException::raise(SmbErrorCode::AlreadyExists,
                                    "Copy Failed: could not claim a duplicate destination after 64 attempts");
            }
        }
    }

    if (isCancelled() || !rootClaimed) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    result_ = destPath;
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
