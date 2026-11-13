#include "CopyItemOperator.hpp"

#include "../../io/SmbCopyTree.hpp"

namespace react_native_smb {

void CopyItemOperator::run() {
    setSourceDestination(fromPath_, toPath_);
    auto handle = requestContext(AcquireMode::Metadata);
    copyTreeImpl(handle, fromPath_, toPath_, recursive_, cancelToken(),
                 [this](double done, double total) { emitProgress(done, total); }, [this](int64_t n) { addExpectedBytes(n); });
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb