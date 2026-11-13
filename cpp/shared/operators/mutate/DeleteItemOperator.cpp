#include "DeleteItemOperator.hpp"

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <deque>
#include <string>
#include <vector>

#include "../../core/SmbModels.hpp"
#include "../../io/PipelinedOps.hpp"
#include "../../io/SmbPathUtil.hpp"
#include "../../io/listing/DirectoryLister.hpp"
#include "../../util/SmbException.hpp"

namespace react_native_smb {

namespace {
constexpr size_t kMaxDeletesInFlight = 16;

// Deletes a listed tree with pipelined requests: files and empty folders at
// once, every folder as soon as its contents are gone, the root folder last.
class TreeDeleter {
   public:
    explicit TreeDeleter(PipelinedOps& ops) : ops_(ops) {}

    void deleteFolder(const std::string& path, const std::vector<SmbFileInfo>& contents) { addFolder(path, contents, nullptr); }

   private:
    struct Folder {
        std::string path;  // as given, for messages
        Folder* parent;
        size_t pending;  // contents not yet deleted
    };

    void addFolder(const std::string& path, const std::vector<SmbFileInfo>& contents, Folder* parent) {
        folders_.push_back(Folder{path, parent, contents.size()});
        Folder* folder = &folders_.back();
        for (const auto& item : contents) {
            if (item.isDirectory)
                addFolder(item.path, item.children, folder);
            else
                removeFile(item.path, folder);
        }
        if (folder->pending == 0) removeFolder(folder);
    }

    void removeFile(const std::string& path, Folder* parent) {
        const std::string norm = path_util::normalized(path);
        ops_.submit([norm](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_unlink_async(ctx, norm.c_str(), cb, data); },
                    [this, path, parent](int status, void*) {
                        if (status < 0) {
                            ops_.failFromErrno(status, "Recursive Delete Failed: Could not delete file '" + path + "'. Error: " + errorText());
                            return;
                        }
                        release(parent);
                    });
    }

    void removeFolder(Folder* folder) {
        const std::string norm = path_util::normalized(folder->path);
        ops_.submit([norm](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_rmdir_async(ctx, norm.c_str(), cb, data); },
                    [this, folder](int status, void*) {
                        if (status < 0) {
                            const std::string what = folder->parent ? "Recursive Delete Failed: Could not delete sub-directory '"
                                                                    : "Delete Failed: Could not delete directory '";
                            ops_.failFromErrno(status, what + folder->path + "'. Error: " + errorText());
                            return;
                        }
                        if (folder->parent) release(folder->parent);
                    });
    }

    void release(Folder* folder) {
        if (folder->pending > 0 && --folder->pending == 0) removeFolder(folder);
    }

    std::string errorText() const { return ops_.ctx() ? smb2_get_error(ops_.ctx()) : "connection lost"; }

    PipelinedOps& ops_;
    std::deque<Folder> folders_;  // stable addresses
};
}  // namespace

void DeleteItemOperator::run() {
    const CancellationToken token = cancelToken();
    const std::string norm = path_util::normalized(path_);
    if (norm.empty()) SmbException::raise(SmbErrorCode::InvalidArgument, "Delete Failed: refusing to delete the share root");
    setSourceDestination(norm, "");

    auto handle = requestContext(AcquireMode::Metadata);
    bool isDirectory = false;
    handle.submitSync([&](smb2_context* ctx) {
        struct smb2_stat_64 st;
        const int result = smb2_stat(ctx, norm.c_str(), &st);
        if (result < 0) {
            SmbException::raiseFromSmb(ctx, result, "Delete Failed: Could not stat '" + path_ + "'. Error: " + smb2_get_error(ctx));
        }
        isDirectory = (st.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
    });

    if (!isDirectory) {
        handle.submitSync([&](smb2_context* ctx) {
            const int r = smb2_unlink(ctx, norm.c_str());
            if (r < 0) SmbException::raiseFromSmb(ctx, r, "Failed to delete file '" + path_ + "': " + smb2_get_error(ctx));
        });
        emitStatus(SmbTaskStatus::Success);
        return;
    }

    listing::DirectoryListOptions options;
    options.maxDepth = -1;
    options.countChildren = false;
    handle.submitSync([&](smb2_context* ctx) {
        const std::vector<SmbFileInfo> tree = listing::listOnCtx(ctx, handle.manager(), path_, options, token);
        if (isCancelled()) return;
        PipelinedOps ops(ctx, handle.manager(), token, kMaxDeletesInFlight, "Delete Failed");
        TreeDeleter deleter(ops);
        deleter.deleteFolder(path_, tree);
        ops.run();
    });
    if (isCancelled()) {
        emitStatus(SmbTaskStatus::Cancelled);
        return;
    }

    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
