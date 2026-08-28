#include "MoveItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <stdexcept>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void MoveItemOperator::run() {

    const std::string from = path_util::normalized(fromPath_);
    const std::string to = path_util::normalized(toPath_);
    if (from.empty()) throw std::invalid_argument("Move Failed: refusing to move the share root");
    if (path_util::isDescendant(from, to)) {
        throw std::invalid_argument("Move Failed: destination cannot be inside the source directory");
    }

    setSourceDestination(from, to);

    auto handle = requestContext(AcquireMode::Interactive);

    handle.submitSync([&](smb2_context* ctx) {
        const int r = smb2_rename(ctx, from.c_str(), to.c_str());
        if (r < 0) {
            throw std::runtime_error("Move Failed: Could not move '" + fromPath_ + "' to '" + toPath_ + "'. Error: " + smb2_get_error(ctx));
        }
    });

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
