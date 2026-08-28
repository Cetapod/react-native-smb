#include "DisconnectOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"

namespace react_native_smb {

void DisconnectOperator::run() {
    pool().disconnect(owningTaskId(), cancelToken());
    if (isCancelled()) return;
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
