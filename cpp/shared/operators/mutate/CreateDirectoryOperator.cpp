#include "CreateDirectoryOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <stdexcept>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbPathUtil.hpp"

namespace react_native_smb {

void CreateDirectoryOperator::run() {

    const std::string norm = path_util::normalized(path_);
    auto handle = requestContext(AcquireMode::Interactive);

    handle.submitSync([&](smb2_context* ctx) {
        const int r = smb2_mkdir(ctx, norm.c_str());
        if (r < 0) {
            throw std::runtime_error("Directory Usage Failed: Could not create directory at '" + path_ + "'. Error: " + smb2_get_error(ctx));
        }
    });

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
