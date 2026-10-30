#include "DeleteItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <vector>

#include "../../ReactNativeSmb.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../io/listing/DirectoryLister.hpp"
#include "../../util/SmbException.hpp"

namespace react_native_smb {

namespace {
// Flattens a listed tree; directories come after their descendants so they can
// be removed in order.
void collectItems(std::vector<SmbFileInfo>& nodes, std::vector<SmbFileInfo>& files, std::vector<SmbFileInfo>& dirs) {
    for (auto& node : nodes) {
        if (!node.isDirectory) {
            files.push_back(std::move(node));
            continue;
        }
        collectItems(node.children, files, dirs);
        node.children.clear();
        dirs.push_back(std::move(node));
    }
}
}  // namespace

void DeleteItemOperator::run() {
    const CancellationToken token = cancelToken();
    const std::string norm = path_util::normalized(path_);
    if (norm.empty()) SmbException::raise(SmbErrorCode::InvalidArgument, "Delete Failed: refusing to delete the share root");
    setSourceDestination(norm, "");

    bool isDirectory = false;
    {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            struct smb2_stat_64 st;
            const int result = smb2_stat(ctx, norm.c_str(), &st);
            if (result < 0) {
                SmbException::raiseFromSmb(ctx, result, "Delete Failed: Could not stat '" + path_ + "'. Error: " + smb2_get_error(ctx));
            }
            isDirectory = (st.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
        });
    }

    if (!isDirectory) {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            const int r = smb2_unlink(ctx, norm.c_str());
            if (r < 0) SmbException::raiseFromSmb(ctx, r, "Failed to delete file '" + path_ + "': " + smb2_get_error(ctx));
        });
        emitStatus(SmbTaskStatus::Success);
        return;
    }

    std::vector<SmbFileInfo> tree;
    {
        listing::DirectoryListOptions options;
        options.maxDepth = -1;
        options.countChildren = false;
        auto handle = requestContext(AcquireMode::Metadata);
        tree = handle.submitSync([&](smb2_context* ctx) { return listing::listOnCtx(ctx, handle.manager(), path_, options, token); });
    }
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    std::vector<SmbFileInfo> fileItems;
    std::vector<SmbFileInfo> dirItems;  // children before their parent
    collectItems(tree, fileItems, dirItems);

    if (!fileItems.empty() && !isCancelled()) {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            for (const auto& item : fileItems) {
                if (isCancelled()) return;
                std::string n = path_util::normalized(item.path);
                const int r = smb2_unlink(ctx, n.c_str());
                if (r < 0) SmbException::raiseFromSmb(ctx, r, "Recursive Delete Failed: Could not delete file '" + item.path + "'. Error: " + smb2_get_error(ctx));
            }
        });
    }
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    {
        auto handle = requestContext(AcquireMode::Metadata);
        handle.submitSync([&](smb2_context* ctx) {
            for (const auto& item : dirItems) {
                if (isCancelled()) return;
                std::string n = path_util::normalized(item.path);
                const int r = smb2_rmdir(ctx, n.c_str());
                if (r < 0) SmbException::raiseFromSmb(ctx, r, "Recursive Delete Failed: Could not delete sub-directory '" + item.path + "'. Error: " + smb2_get_error(ctx));
            }
            const int rootR = smb2_rmdir(ctx, norm.c_str());
            if (rootR < 0) SmbException::raiseFromSmb(ctx, rootR, "Delete Failed: Could not delete directory '" + path_ + "'. Error: " + smb2_get_error(ctx));
        });
    }

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
