#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../core/CancellationToken.hpp"
#include "SmbPathUtil.hpp"

namespace react_native_smb {

// Directory listing on a single smb2_context. Does not use the connection pool.
inline std::vector<SmbFileInfo> listOnCtx(smb2_context* ctx, const std::string& path, bool recursive, int maxDepth, const CancellationToken& cancel) {
    std::vector<SmbFileInfo> files;
    const std::string norm = path_util::normalized(path);

    smb2dir* dir = smb2_opendir(ctx, norm.c_str());
    if (!dir) {
        throw std::runtime_error(std::string("List Directory Failed: Could not open directory '") + path + "'. Error: " + smb2_get_error(ctx));
    }

    try {
        struct smb2dirent* ent;
        while ((ent = smb2_readdir(ctx, dir))) {
            if (cancel.cancelled()) {
                smb2_closedir(ctx, dir);
                return {};
            }
            std::string name = ent->name;
            if (name == "." || name == "..") continue;
            std::string fullPath = path_util::buildUrl(path, name);
            files.push_back(path_util::makeFileInfo(ent->st, name, fullPath));
        }
    } catch (...) {
        smb2_closedir(ctx, dir);
        throw;
    }
    smb2_closedir(ctx, dir);

    // Cheap childCount per directory (opendir/readdir count, no per-entry stat).
    for (auto& item : files) {
        if (!item.isDirectory) continue;
        if (cancel.cancelled()) return {};
        smb2dir* sub = smb2_opendir(ctx, path_util::normalized(item.path).c_str());
        if (!sub) {
            item.childCount = -1;
            continue;
        }
        int64_t count = 0;
        struct smb2dirent* subEnt;
        while ((subEnt = smb2_readdir(ctx, sub))) {
            std::string n = subEnt->name;
            if (n == "." || n == "..") continue;
            ++count;
        }
        smb2_closedir(ctx, sub);
        item.childCount = count;
    }

    if (recursive && maxDepth != 0) {
        for (auto& item : files) {
            if (cancel.cancelled()) return {};
            if (item.isDirectory) {
                try {
                    int newMaxDepth = (maxDepth > 0) ? maxDepth - 1 : maxDepth;
                    item.children = listOnCtx(ctx, item.path, true, newMaxDepth, cancel);
                } catch (const std::exception&) {
                    item.children.clear();
                }
            }
        }
    }
    return files;
}

}  // namespace react_native_smb
