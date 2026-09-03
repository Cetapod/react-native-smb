#include "UploadFileOperator.hpp"

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbFileIO.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void UploadFileOperator::run() {

    const std::string remote = path_util::normalized(remotePath_);
    const std::string local = path_util::convertUriToPath(localPath_);

    setSourceDestination(local, remote);  // local -> remote for upload

    auto handle = requestContext(AcquireMode::Metadata);

    const CancellationToken token = cancelToken();
    handle.submitSync([&](smb2_context* ctx) {
        smbWriteFileAsync(
            ctx, handle.manager(), local, remote, owningTaskId(), [this](double done, double total) { emitProgress(done, total); }, token);
    });

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
