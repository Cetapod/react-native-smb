#include "ListDirectoryOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbDirScan.hpp"

namespace react_native_smb {

void ListDirectoryOperator::run() {

    const AcquireMode mode = (!recursive_ || maxDepth_ == 0) ? AcquireMode::Interactive : AcquireMode::Metadata;
    auto handle = requestContext(mode);

    const CancellationToken token = cancelToken();
    result_ = handle.submitSync([&](smb2_context* ctx) { return listOnCtx(ctx, path_, recursive_, maxDepth_, token); });
    publishThisResult();

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
