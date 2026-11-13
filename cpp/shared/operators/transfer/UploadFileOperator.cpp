#include "UploadFileOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbFileIO.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void UploadFileOperator::prepare() {
    local_ = path_util::convertUriToPath(localPath_);
    remote_ = path_util::normalized(remotePath_);
    setSourceDestination(local_, remote_);  // local -> remote for upload
}

void UploadFileOperator::run() {
    auto& handle = context();

    const CancellationToken token = cancelToken();
    handle.submitSync([&](smb2_context* ctx) {
        smbWriteFileAsync(
            ctx, handle.manager(), local_, remote_, owningTaskId(), [this](double done, double total) { emitProgress(done, total); }, token);
    });

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
