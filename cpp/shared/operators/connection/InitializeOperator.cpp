#include "InitializeOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"

namespace react_native_smb {

void InitializeOperator::run() {
    pool().initialize(url_, credentials_, owningTaskId(), cancelToken());
    if (isCancelled()) return;
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
