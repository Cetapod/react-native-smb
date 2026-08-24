#include "GetSecurityDescriptorOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbSecurityQuery.hpp"

namespace react_native_smb {

void GetSecurityDescriptorOperator::run() {
    emitStatus(SmbTaskStatus::Running);

    auto handle = requestContext(AcquireMode::Interactive);
    result_ = handle.submitSync([&](smb2_context* ctx) { return querySecurityDescriptorOnCtx(ctx, path_, handle.manager()); });

    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
