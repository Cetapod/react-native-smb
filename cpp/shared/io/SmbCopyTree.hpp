#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

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
#include "SmbDirScan.hpp"
#include "SmbPathUtil.hpp"
#include "SmbTreeOps.hpp"
#include "transfer/SmbFileCopyInternal.hpp"

namespace react_native_smb {

struct CopyTreeSourceInfo {
    bool isDirectory = false;
    int64_t totalSize = 0;
};

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
        source.totalSize = tree_ops::calculateTotalSize(ctx, normalizedPath, source.isDirectory && recursive, cancel);
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

    const CopyTreeSourceInfo source =
        preparedSource ? *preparedSource : inspectCopyTreeSource(*sharedLease, fromPath, recursive, cancel);

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

    struct CopyJob {
        std::string src;
        std::string dst;
        int64_t size;
    };
    std::vector<CopyJob> files;
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

        std::function<void(const std::string&, const std::string&)> walk = [&](const std::string& srcDir,
                                                                               const std::string& dstDir) {
            if (cancel.cancelled()) return;
            std::vector<SmbFileInfo> contents = listOnCtx(ctx, srcDir, false, 0, cancel);
            for (const auto& item : contents) {
                if (cancel.cancelled()) return;
                std::string destItem = path_util::buildUrl(dstDir, item.name);
                if (item.isDirectory) {
                    std::string normDest = path_util::normalized(destItem);
                    const int r = smb2_mkdir(ctx, normDest.c_str());
                    if (r < 0) {
                        SmbException::raiseFromSmb(
                            ctx,
                            r, "Failed to create destination directory: " + std::string(smb2_get_error(ctx)));
                    }
                    walk(item.path, destItem);
                } else {
                    files.push_back({item.path, destItem, item.size});
                }
            }
        };
        walk(fromPath, toPath);
    });

    if (cancel.cancelled()) return;
    if (files.empty()) {
        if (onProgress) {
            onProgress(static_cast<double>(source.totalSize), static_cast<double>(source.totalSize));
        }
        return;
    }

    for (const auto& job : files) {
        if (cancel.cancelled()) break;
        sharedLease->submitSync([&](smb2_context* ctx) {
            smbCopyFileAsync(ctx, sharedLease->manager(), path_util::normalized(job.src),
                             path_util::normalized(job.dst), totalBytesCopied, source.totalSize, onProgress, cancel);
        });
    }

    if (cancel.cancelled()) return;
    if (onProgress) {
        onProgress(static_cast<double>(source.totalSize), static_cast<double>(source.totalSize));
    }
}

}  // namespace react_native_smb
