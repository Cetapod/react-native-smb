#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <cctype>
#include <string>
#include <vector>

#include "../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../core/CancellationToken.hpp"
#include "../util/SmbException.hpp"
#include "SmbPathUtil.hpp"
#include "listing/DirectoryLister.hpp"

namespace react_native_smb {

// Naming helper for the duplicate operator.
namespace tree_ops {

// Generate a unique "<base> copy[ N]<ext>" name not present in parentPath.
inline std::string generateUniqueCopyName(smb2_context* ctx, SmbConnectionManager& manager, const std::string& parentPath,
                                          const std::string& originalName, const CancellationToken& cancel, int& nextIndex) {
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

    // Only "<base> copy…" names can collide, so let the server filter the parent.
    // The pattern may match more (wildcards in the name); exact comparison follows.
    listing::DirectoryListOptions options;
    options.countChildren = false;
    options.pattern = baseName + " copy*";
    std::vector<SmbFileInfo> existingFiles = listing::listOnCtx(ctx, manager, parentPath, options, cancel);
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
