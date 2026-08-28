#include "DownloadFileOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbFileIO.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void DownloadFileOperator::run() {

    const std::string remote = path_util::normalized(remotePath_);
    const std::string local = path_util::convertUriToPath(localPath_);

    setSourceDestination(remote, local);

    auto handle = requestContext(AcquireMode::Metadata);

    const CancellationToken token = cancelToken();
    handle.submitSync([&](smb2_context* ctx) {
        smbReadFileAsync(
            ctx, handle.manager(), remote, local, [this](double done, double total) { emitProgress(done, total); }, token);
    });

    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
