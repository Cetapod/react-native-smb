#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../ReactNativeSmb.hpp"
#include "../connection/MetadataContextLease.hpp"
#include "../connection/SmbConnectionPool.hpp"
#include "../core/CancellationToken.hpp"
#include "../core/SmbEnums.hpp"
#include "SmbFileIO.hpp"
#include "SmbDirScan.hpp"
#include "SmbPathUtil.hpp"
#include "SmbTreeOps.hpp"

namespace react_native_smb {

inline void copyTreeImpl(SmbConnectionPool& pool, SmbOperatorKind kind, const std::string& fromPath, const std::string& toPath, bool recursive,
                         const CancellationToken& cancel, const std::function<void(double, double)>& onProgress,
                         const std::function<void(int64_t)>& onExpectedBytes, const std::string& taskId = "",
                         MetadataContextLease* sharedLease = nullptr) {
    const std::string normFrom = path_util::normalized(fromPath);
    const std::string normTo = path_util::normalized(toPath);
    if ((normFrom.empty() && !normTo.empty()) || path_util::isDescendant(normFrom, normTo)) {
        throw std::invalid_argument("Copy Failed: destination cannot be inside the source directory");
    }

    std::optional<MetadataContextLease> ownedLease;
    if (!sharedLease) {
        ownedLease.emplace(pool, kind, taskId);
        sharedLease = &*ownedLease;
    }

    bool isDirectory = false;
    int64_t totalSize = 0;
    sharedLease->submitSync([&](smb2_context* ctx) {
        struct smb2_stat_64 st;
        if (smb2_stat(ctx, normFrom.c_str(), &st) < 0) {
            throw std::runtime_error("Copy Failed: Could not stat source '" + fromPath + "'. Error: " + smb2_get_error(ctx));
        }
        isDirectory = (st.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
        totalSize = tree_ops::calculateTotalSize(ctx, fromPath, isDirectory && recursive, cancel);
    });

    auto totalBytesCopied = std::make_shared<int64_t>(0);
    if (onExpectedBytes) onExpectedBytes(totalSize);
    if (onProgress) onProgress(0.0, static_cast<double>(totalSize));

    if (!isDirectory) {
        sharedLease->submitSync([&](smb2_context* ctx) {
            smbCopyFileAsync(ctx, sharedLease->manager(), path_util::normalized(fromPath), path_util::normalized(toPath), totalBytesCopied, totalSize, onProgress, cancel);
        });
        if (onProgress) onProgress(static_cast<double>(totalSize), static_cast<double>(totalSize));
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
        const int mk = smb2_mkdir(ctx, normToRoot.c_str());
        if (mk < 0) throw std::runtime_error("Failed to create destination directory: " + std::string(smb2_get_error(ctx)));
        if (!recursive) return;

        std::function<void(const std::string&, const std::string&)> walk = [&](const std::string& srcDir, const std::string& dstDir) {
            if (cancel.cancelled()) return;
            std::vector<SmbFileInfo> contents = listOnCtx(ctx, srcDir, false, 0, cancel);
            for (const auto& item : contents) {
                if (cancel.cancelled()) return;
                std::string destItem = path_util::buildUrl(dstDir, item.name);
                if (item.isDirectory) {
                    std::string normDest = path_util::normalized(destItem);
                    const int r = smb2_mkdir(ctx, normDest.c_str());
                    if (r < 0) throw std::runtime_error("Failed to create destination directory: " + std::string(smb2_get_error(ctx)));
                    walk(item.path, destItem);
                } else {
                    files.push_back({item.path, destItem, item.size});
                }
            }
        };
        walk(fromPath, toPath);
    });

    if (cancel.cancelled() || files.empty()) {
        if (onProgress) onProgress(static_cast<double>(totalSize), static_cast<double>(totalSize));
        return;
    }

    for (const auto& job : files) {
        if (cancel.cancelled()) break;
        sharedLease->submitSync([&](smb2_context* ctx) {
            smbCopyFileAsync(ctx, sharedLease->manager(), path_util::normalized(job.src), path_util::normalized(job.dst), totalBytesCopied, totalSize, onProgress, cancel);
        });
    }

    if (onProgress) onProgress(static_cast<double>(totalSize), static_cast<double>(totalSize));
}

}  // namespace react_native_smb
