#include "ConnectOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"

namespace react_native_smb {

void ConnectOperator::run() {
    pool().connect(url_, credentials_);
    result_ = SmbConnectionInfo{
        pool().getCurrentUrl(),
        pool().getServerName(),
        pool().getShareName(),
        pool().isConnected(),
    };
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
