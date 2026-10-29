#include "GetPathInfoOperator.hpp"

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <string>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../io/SmbSecurityQuery.hpp"
#include "../../io/SmbServiceLoop.hpp"
#include "../../io/listing/DirectoryLister.hpp"
#include "../../util/SmbException.hpp"

namespace react_native_smb {

namespace {
// Outlives every libsmb2 callback of the stat compound (see serviceUntil below).
struct StatQuery {
    smb2_stat_64 st{};
    int status{0};
    std::string error;
    bool finished{false};
};

void onStat(smb2_context* smb2, int status, void* /*commandData*/, void* privateData) {
    auto* query = static_cast<StatQuery*>(privateData);
    query->status = status;
    if (status < 0) query->error = smb2_get_error(smb2);
    query->finished = true;
}
}  // namespace

// Stat and security descriptor are queued together and complete in one round
// trip; a folder then reads its own entries once for childCount. Links resolve
// to their target, like smb2_stat elsewhere.
void GetPathInfoOperator::run() {
    const std::string norm = path_util::normalized(path_);
    const CancellationToken token = cancelToken();
    auto handle = requestContext(AcquireMode::Interactive);

    SmbFileInfo info = handle.submitSync([&](smb2_context* ctx) {
        StatQuery stat;
        SecurityDescriptorQuery sd;
        if (smb2_stat_async(ctx, norm.c_str(), &stat.st, onStat, &stat) != 0) {
            SmbException::raiseFromSmb(ctx, -1, "Get Path Info Failed: could not queue stat for '" + path_ + "'. Error: " + smb2_get_error(ctx));
        }
        const bool sdQueued = queueSecurityDescriptorQuery(ctx, norm, sd) == 0;
        try {
            serviceUntil(ctx, [&] { return stat.finished && (!sdQueued || sd.finished); }, "Get Path Info");
        } catch (...) {
            handle.manager().invalidateContext();
            throw;
        }

        if (stat.status < 0) {
            const std::string message = "Get Path Info Failed: '" + path_ + "': " + stat.error;
            throw SmbException(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(stat.status, 0, message)), message);
        }
        if (!sdQueued) {
            SmbException::raise(SmbErrorCode::Io, "Get Path Info Failed: could not queue security descriptor query for '" + path_ + "'");
        }

        SmbFileInfo result = path_util::makeFileInfo(stat.st, path_util::extractFileName(norm), path_);
        if (!sd.failed()) {
            result.securityDescriptor = sd.result;
        } else if (sd.errorCode() != SmbErrorCode::AccessDenied) {
            throw SmbException(sd.errorCode(), sd.error);
        }

        if (result.isDirectory) {
            listing::DirectoryListOptions options;
            options.countChildren = false;
            result.childCount = static_cast<int64_t>(listing::listOnCtx(ctx, handle.manager(), path_, options, token).size());
        }
        return result;
    });
    if (isCancelled()) return;

    result_ = std::move(info);
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
