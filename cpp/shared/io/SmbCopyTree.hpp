#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../ReactNativeSmb.hpp"
#include "../connection/MetadataContextLease.hpp"
#include "../connection/SmbConnectionPool.hpp"
#include "../core/CancellationToken.hpp"
#include "../core/SmbEnums.hpp"
#include "../util/SmbException.hpp"
#include "PipelinedOps.hpp"
#include "SmbPathUtil.hpp"
#include "listing/DirectoryLister.hpp"
#include "transfer/SmbConcurrentFileCopy.hpp"
#include "transfer/SmbFileCopyInternal.hpp"

namespace react_native_smb {

struct CopyTreeSourceInfo {
    bool isDirectory = false;
    int64_t totalSize = 0;
    // Recursive directory copies: the source tree, listed once and reused for the copy.
    std::vector<SmbFileInfo> entries;
};

namespace copy_tree_detail {
constexpr size_t kMaxFoldersInFlight = 16;
constexpr size_t kMaxFilesInFlight = 8;

struct CopyJob {
    std::string src;
    std::string dst;
    int64_t size;
};

// Every file of a listed tree with its destination path.
inline void collectFiles(const std::vector<SmbFileInfo>& items, const std::string& dstDir, std::vector<CopyJob>& files) {
    for (const auto& item : items) {
        const std::string dest = path_util::buildUrl(dstDir, item.name);
        if (item.isDirectory)
            collectFiles(item.children, dest, files);
        else
            files.push_back({item.path, dest, item.size});
    }
}

inline int64_t totalFileSize(const std::vector<SmbFileInfo>& entries) {
    int64_t total = 0;
    for (const auto& entry : entries) total += entry.isDirectory ? totalFileSize(entry.children) : entry.size;
    return total;
}
}  // namespace copy_tree_detail

inline CopyTreeSourceInfo inspectCopyTreeSource(MetadataContextLease& lease, const std::string& fromPath,
                                                bool recursive, const CancellationToken& cancel) {
    CopyTreeSourceInfo source;
    lease.submitSync([&](smb2_context* ctx) {
        const std::string normalizedPath = path_util::normalized(fromPath);
        smb2_stat_64 st{};
        const int result = smb2_stat(ctx, normalizedPath.c_str(), &st);
        if (result < 0) {
            SmbException::raiseFromSmb(
                ctx, result, "Copy Failed: Could not stat source '" + fromPath + "'. Error: " + smb2_get_error(ctx));
        }
        source.isDirectory = (st.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
        if (!source.isDirectory) {
            source.totalSize = static_cast<int64_t>(st.smb2_size);
            return;
        }
        if (!recursive) return;

        listing::DirectoryListOptions options;
        options.maxDepth = -1;
        options.countChildren = false;
        source.entries = listing::listOnCtx(ctx, lease.manager(), fromPath, options, cancel);
        source.totalSize = copy_tree_detail::totalFileSize(source.entries);
    });
    return source;
}

inline void copyTreeImpl(SmbConnectionPool& pool, SmbOperatorKind kind, const std::string& fromPath,
                         const std::string& toPath, bool recursive, const CancellationToken& cancel,
                         const std::function<void(double, double)>& onProgress,
                         const std::function<void(int64_t)>& onExpectedBytes, const std::string& taskId = "",
                         MetadataContextLease* sharedLease = nullptr,
                         const CopyTreeSourceInfo* preparedSource = nullptr,
                         const std::function<void()>& onRootClaimed = {}) {
    const std::string normFrom = path_util::normalized(fromPath);
    const std::string normTo = path_util::normalized(toPath);
    if ((normFrom.empty() && !normTo.empty()) || path_util::isDescendant(normFrom, normTo)) {
        SmbException::raise(SmbErrorCode::InvalidArgument, "Copy Failed: destination cannot be inside the source directory");
    }

    std::optional<MetadataContextLease> ownedLease;
    if (!sharedLease) {
        ownedLease.emplace(pool, kind, taskId);
        sharedLease = &*ownedLease;
    }

    CopyTreeSourceInfo inspected;
    if (!preparedSource) inspected = inspectCopyTreeSource(*sharedLease, fromPath, recursive, cancel);
    const CopyTreeSourceInfo& source = preparedSource ? *preparedSource : inspected;

    auto totalBytesCopied = std::make_shared<int64_t>(0);
    auto rootClaimed = [&] {
        if (onRootClaimed) {
            onRootClaimed();
        }
        if (onExpectedBytes) {
            onExpectedBytes(source.totalSize);
        }
        if (onProgress) {
            onProgress(0.0, static_cast<double>(source.totalSize));
        }
    };

    if (!source.isDirectory) {
        if (cancel.cancelled()) {
            SmbException::raise(SmbErrorCode::Cancelled, "Copy cancelled");
        }
        sharedLease->submitSync([&](smb2_context* ctx) {
            smbCopyFileAsync(ctx, sharedLease->manager(), normFrom, normTo, totalBytesCopied, source.totalSize,
                             onProgress, cancel, rootClaimed);
        });
        if (onProgress) {
            onProgress(static_cast<double>(source.totalSize), static_cast<double>(source.totalSize));
        }
        return;
    }

    std::vector<copy_tree_detail::CopyJob> files;
    const std::string normToRoot = normTo;
    sharedLease->submitSync([&](smb2_context* ctx) {
        if (cancel.cancelled()) {
            SmbException::raise(SmbErrorCode::Cancelled, "Copy cancelled");
        }
        const int mk = smb2_mkdir(ctx, normToRoot.c_str());
        if (mk < 0) {
            SmbException::raiseFromSmb(
                ctx,
                mk, "Failed to create destination directory: " + std::string(smb2_get_error(ctx)));
        }
        try {
            rootClaimed();
        } catch (...) {
            smb2_rmdir(ctx, normToRoot.c_str());
            throw;
        }
        if (!recursive) return;

        copy_tree_detail::collectFiles(source.entries, toPath, files);

        // Folders in parallel; each one's subfolders once it exists.
        PipelinedOps ops(ctx, sharedLease->manager(), cancel, copy_tree_detail::kMaxFoldersInFlight, "Copy Failed");
        std::function<void(const std::vector<SmbFileInfo>&, const std::string&)> createFolders =
            [&](const std::vector<SmbFileInfo>& items, const std::string& dstDir) {
                for (const auto& item : items) {
                    if (!item.isDirectory) continue;
                    const std::string dest = path_util::buildUrl(dstDir, item.name);
                    const std::string norm = path_util::normalized(dest);
                    const std::vector<SmbFileInfo>* children = &item.children;
                    ops.submit([norm](smb2_context* c, smb2_command_cb cb, void* data) { return smb2_mkdir_async(c, norm.c_str(), cb, data); },
                               [&, children, dest](int status, void*) {
                                   if (status < 0) {
                                       ops.failFromErrno(status, "Failed to create destination directory: " +
                                                                     std::string(ops.ctx() ? smb2_get_error(ops.ctx()) : "connection lost"));
                                       return;
                                   }
                                   createFolders(*children, dest);
                               });
                }
            };
        createFolders(source.entries, toPath);
        ops.run();
    });

    if (cancel.cancelled()) return;
    if (files.empty()) {
        if (onProgress) {
            onProgress(static_cast<double>(source.totalSize), static_cast<double>(source.totalSize));
        }
        return;
    }

    const auto copyOne = [&](const copy_tree_detail::CopyJob& job) {
        sharedLease->submitSync([&](smb2_context* ctx) {
            smbCopyFileAsync(ctx, sharedLease->manager(), path_util::normalized(job.src),
                             path_util::normalized(job.dst), totalBytesCopied, source.totalSize, onProgress, cancel);
        });
    };

    // The first non-empty file goes alone: it finds out whether the server
    // supports server-side copy (copying through the client if not).
    auto probe = std::find_if(files.begin(), files.end(), [](const copy_tree_detail::CopyJob& job) { return job.size > 0; });
    if (probe == files.end()) probe = files.begin();
    copyOne(*probe);
    files.erase(probe);
    if (cancel.cancelled() || files.empty()) {
        if (!cancel.cancelled() && onProgress) {
            onProgress(static_cast<double>(source.totalSize), static_cast<double>(source.totalSize));
        }
        return;
    }

    if (sharedLease->manager().capabilities().serverSideCopyUnsupported.load()) {
        for (const auto& job : files) {
            if (cancel.cancelled()) break;
            copyOne(job);
        }
    } else {
        std::vector<detail::FileCopyJob> jobs;
        jobs.reserve(files.size());
        for (const auto& job : files) jobs.push_back({path_util::normalized(job.src), path_util::normalized(job.dst)});
        sharedLease->submitSync([&](smb2_context* ctx) {
            detail::copyFilesServerSide(ctx, sharedLease->manager(), jobs, copy_tree_detail::kMaxFilesInFlight, totalBytesCopied,
                                        source.totalSize, onProgress, cancel);
        });
    }

    if (cancel.cancelled()) return;
    if (onProgress) {
        onProgress(static_cast<double>(source.totalSize), static_cast<double>(source.totalSize));
    }
}

}  // namespace react_native_smb
