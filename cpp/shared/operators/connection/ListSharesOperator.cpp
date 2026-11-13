#include "ListSharesOperator.hpp"

#include "../../connection/SmbConnection.hpp"

namespace react_native_smb {

void ListSharesOperator::run() {
    auto handle = requestContext(AcquireMode::Interactive);
    result_ = handle.manager().listShares(owningTaskId());
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
