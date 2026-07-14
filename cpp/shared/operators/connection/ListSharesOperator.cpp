#include "ListSharesOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"

namespace react_native_smb {

void ListSharesOperator::run() {
    result_ = pool().listShares(owningTaskId());
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
