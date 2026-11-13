#include "DownloadFileOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbFileIO.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void DownloadFileOperator::prepare() {
    remote_ = path_util::normalized(remotePath_);
    local_ = path_util::convertUriToPath(localPath_);
    setSourceDestination(remote_, local_);
}

void DownloadFileOperator::run() {
    auto& handle = context();

    const CancellationToken token = cancelToken();
    handle.submitSync([&](smb2_context* ctx) {
        smbReadFileAsync(
            ctx, handle.manager(), remote_, local_, owningTaskId(), [this](double done, double total) { emitProgress(done, total); }, token);
    });

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
