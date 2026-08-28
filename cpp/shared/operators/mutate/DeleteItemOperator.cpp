#include "DeleteItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "../../ReactNativeSmb.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../io/SmbTreeOps.hpp"

namespace react_native_smb {

void DeleteItemOperator::run() {
    const CancellationToken token = cancelToken();
    const std::string norm = path_util::normalized(path_);
    if (norm.empty()) throw std::invalid_argument("Delete Failed: refusing to delete the share root");
    setSourceDestination(norm, "");

    bool isDirectory = false;
    {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            struct smb2_stat_64 st;
            if (smb2_stat(ctx, norm.c_str(), &st) < 0) {
                throw std::runtime_error("Delete Failed: Could not stat '" + path_ + "'. Error: " + smb2_get_error(ctx));
            }
            isDirectory = (st.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
        });
    }

    if (!isDirectory) {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            const int r = smb2_unlink(ctx, norm.c_str());
            if (r < 0) throw std::runtime_error("Failed to delete file '" + path_ + "': " + smb2_get_error(ctx));
        });
        emitStatus(SmbTaskStatus::Success);
        return;
    }

    std::vector<SmbFileInfo> allItems;
    {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) { tree_ops::collectAllItems(ctx, path_, allItems, token); });
    }
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    std::vector<SmbFileInfo> fileItems;
    std::vector<SmbFileInfo> dirItems;
    for (auto& item : allItems) {
        if (item.isDirectory)
            dirItems.push_back(std::move(item));
        else
            fileItems.push_back(std::move(item));
    }

    if (!fileItems.empty() && !isCancelled()) {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            for (const auto& item : fileItems) {
                if (isCancelled()) return;
                std::string n = path_util::normalized(item.path);
                const int r = smb2_unlink(ctx, n.c_str());
                if (r < 0) throw std::runtime_error("Recursive Delete Failed: Could not delete file '" + item.path + "'. Error: " + smb2_get_error(ctx));
            }
        });
    }
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    std::sort(dirItems.begin(), dirItems.end(), [](const SmbFileInfo& a, const SmbFileInfo& b) { return a.path.length() > b.path.length(); });
    {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            for (const auto& item : dirItems) {
                if (isCancelled()) return;
                std::string n = path_util::normalized(item.path);
                const int r = smb2_rmdir(ctx, n.c_str());
                if (r < 0) throw std::runtime_error("Recursive Delete Failed: Could not delete sub-directory '" + item.path + "'. Error: " + smb2_get_error(ctx));
            }
            const int rootR = smb2_rmdir(ctx, norm.c_str());
            if (rootR < 0) throw std::runtime_error("Delete Failed: Could not delete directory '" + path_ + "'. Error: " + smb2_get_error(ctx));
        });
    }

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
