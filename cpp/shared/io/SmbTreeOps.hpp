#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <cctype>
#include <string>
#include <vector>

#include "../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../core/CancellationToken.hpp"
#include "../util/SmbException.hpp"
#include "SmbDirScan.hpp"
#include "SmbPathUtil.hpp"

namespace react_native_smb {

// Tree helpers for delete, copy, and duplicate operators.
namespace tree_ops {

// Depth-first collect of every descendant (children appended before parent so
// callers can rmdir deepest-first). Skips inaccessible sub-dirs.
inline void collectAllItems(smb2_context* ctx, const std::string& path, std::vector<SmbFileInfo>& allItems, const CancellationToken& cancel) {
    try {
        std::vector<SmbFileInfo> contents = listOnCtx(ctx, path, false, 0, cancel);
        for (const auto& item : contents) {
            if (item.isDirectory) collectAllItems(ctx, item.path, allItems, cancel);
            allItems.push_back(item);
        }
    } catch (const std::exception&) {
        // Permission errors on sub-dirs: skip; parent rmdir reports "not empty".
    }
}

// Recursively total the byte size of a path. Returns partial size on errors.
inline int64_t calculateTotalSize(smb2_context* ctx, const std::string& path, bool recursive, const CancellationToken& cancel) {
    const std::string norm = path_util::normalized(path);
    struct smb2_stat_64 stat;
    if (smb2_stat(ctx, norm.c_str(), &stat) < 0) return 0;

    const bool isDirectory = (stat.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
    if (!isDirectory) return static_cast<int64_t>(stat.smb2_size);
    if (!recursive) return 0;

    int64_t total = 0;
    try {
        std::vector<SmbFileInfo> contents = listOnCtx(ctx, path, false, 0, cancel);
        for (const auto& item : contents) {
            if (item.isDirectory)
                total += calculateTotalSize(ctx, item.path, true, cancel);
            else
                total += item.size;
        }
    } catch (const std::exception&) {
        // Ignore inaccessible directories; return partial size.
    }
    return total;
}

// Generate a unique "<base> copy[ N]<ext>" name not present in parentPath.
inline std::string generateUniqueCopyName(smb2_context* ctx, const std::string& parentPath, const std::string& originalName,
                                          const CancellationToken& cancel, int& nextIndex) {
    size_t lastDot = originalName.find_last_of('.');
    std::string baseName = (lastDot != std::string::npos && lastDot > 0) ? originalName.substr(0, lastDot) : originalName;
    std::string extension = (lastDot != std::string::npos && lastDot > 0) ? originalName.substr(lastDot) : "";

    // Strip a previous " copy" / " copy N" suffix so copies-of-copies stay tidy.
    const std::string copySuffix = " copy";
    size_t copyPos = baseName.rfind(copySuffix);
    if (copyPos != std::string::npos) {
        std::string remainder = baseName.substr(copyPos + copySuffix.length());
        bool isClean = false;
        if (remainder.empty()) {
            isClean = true;
        } else if (remainder[0] == ' ') {
            isClean = true;
            for (size_t i = 1; i < remainder.length(); i++) {
                if (!std::isdigit(static_cast<unsigned char>(remainder[i]))) {
                    isClean = false;
                    break;
                }
            }
        }
        if (isClean) baseName = baseName.substr(0, copyPos);
    }

    std::vector<SmbFileInfo> existingFiles = listOnCtx(ctx, parentPath, false, 0, cancel);
    std::vector<std::string> existingNames;
    existingNames.reserve(existingFiles.size());
    for (const auto& item : existingFiles) existingNames.push_back(item.name);

    constexpr int kMaxCopyNameIndex = 10000;
    for (int counter = nextIndex; counter <= kMaxCopyNameIndex; ++counter) {
        std::string candidate = baseName + " copy";
        if (counter > 1) {
            candidate += " " + std::to_string(counter);
        }
        candidate += extension;

        bool found = false;
        for (const auto& existing : existingNames) {
            if (existing == candidate) {
                found = true;
                break;
            }
        }
        if (!found) {
            nextIndex = counter + 1;
            return candidate;
        }
    }

    SmbException::raise(SmbErrorCode::AlreadyExists,
                        "Copy Failed: no available duplicate name for '" + originalName + "'");
}

}  // namespace tree_ops
}  // namespace react_native_smb
