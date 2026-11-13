#include "RenameItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../util/SmbException.hpp"

namespace react_native_smb {

void RenameItemOperator::prepare() {
    if (newName_.empty() || newName_.find('/') != std::string::npos || newName_.find('\\') != std::string::npos) {
        SmbException::raise(SmbErrorCode::InvalidArgument, "Rename Failed: invalid new name '" + newName_ + "'");
    }
    from_ = path_util::normalized(currentPath_);
    to_ = path_util::normalized(path_util::buildUrl(path_util::parentOf(currentPath_), newName_));
    setSourceDestination(from_, to_);
}

void RenameItemOperator::run() {
    auto& handle = context();

    handle.submitSync([&](smb2_context* ctx) {
        const int r = smb2_rename(ctx, from_.c_str(), to_.c_str());
        if (r < 0) {
            SmbException::raiseFromSmb(ctx, r, "Rename Failed: Could not rename '" + currentPath_ + "' to '" + newName_ + "'. Error: " + smb2_get_error(ctx));
        }
    });

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
