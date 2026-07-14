#include "DuplicateItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <string>

#include "../../io/SmbCopyTree.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../io/SmbTreeOps.hpp"

namespace react_native_smb {

void DuplicateItemOperator::run() {
    const CancellationToken token = cancelToken();

    const std::string parentPath = path_util::parentOf(path_);
    const std::string fileName = path_util::extractFileName(path_);

    MetadataContextLease lease = makeMetadataLease();

    std::string destPath;
    lease.submitSync([&](smb2_context* ctx) {
        std::string newName = tree_ops::generateUniqueCopyName(ctx, parentPath, fileName, token);
        destPath = path_util::buildUrl(parentPath, newName);
    });

    if (isCancelled() || destPath.empty()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    setSourceDestination(path_, destPath);

    copyTreeImpl(pool(), kind(), path_, destPath, /*recursive=*/true, token, [this](double done, double total) { emitProgress(done, total); },
                 [this](int64_t n) { addExpectedBytes(n); }, owningTaskId(), &lease);

    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    result_ = destPath;
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb