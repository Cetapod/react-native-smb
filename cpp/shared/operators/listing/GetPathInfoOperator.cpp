#include "GetPathInfoOperator.hpp"

#include <stdexcept>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbDirScan.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void GetPathInfoOperator::run() {

    const std::string parentPath = path_util::parentOf(path_);
    const std::string fileName = path_util::extractFileName(path_);

    auto handle = requestContext(AcquireMode::Interactive);

    const CancellationToken token = cancelToken();
    bool found = false;
    SmbFileInfo info;
    try {
        std::vector<SmbFileInfo> siblings = handle.submitSync([&](smb2_context* ctx) { return listOnCtx(ctx, parentPath, true, 1, token); });
        for (const auto& item : siblings) {
            if (item.name == fileName) {
                info = item;
                found = true;
                break;
            }
        }
    } catch (...) {
        throw std::runtime_error("Failed to get path info for '" + path_ + "': File not found.");
    }

    if (!found) {
        throw std::runtime_error("Failed to get path info for '" + path_ + "': File not found.");
    }

    result_ = info;
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
