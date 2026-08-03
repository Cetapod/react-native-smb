#include "ListDirectoryOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbDirScan.hpp"
#include "../../io/SmbParallelDirScan.hpp"

namespace react_native_smb {

void ListDirectoryOperator::run() {
    const CancellationToken token = cancelToken();

    if (!recursive_ || maxDepth_ == 0) {
        auto handle = requestContext(AcquireMode::Interactive);
        result_ = handle.submitSync([&](smb2_context* ctx) { return scanDirectoryOnCtx(ctx, path_, token); });
    } else {
        result_ = listDirectoryParallel(pool(), owningTaskId(), path_, maxDepth_, token);
    }

    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
