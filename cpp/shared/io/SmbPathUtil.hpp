#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../util/SmbException.hpp"

namespace react_native_smb {

// Path and stat helpers shared by operators.
namespace path_util {

// Normalize a remote path into libsmb2's root-relative form. SMB path case is
// server-defined, so canonicalization intentionally does not change case.
inline std::string normalized(const std::string& path) {
    std::vector<std::string> components;
    std::string component;

    for (size_t i = 0; i <= path.size(); ++i) {
        const char c = i < path.size() ? path[i] : '/';
        if (c != '/' && c != '\\') {
            component += c;
            continue;
        }
        if (component.empty() || component == ".") {
            component.clear();
            continue;
        }
        if (component == "..") {
            if (components.empty()) SmbException::raise(SmbErrorCode::InvalidArgument, "SMB path escapes the share root");
            components.pop_back();
        } else {
            components.push_back(std::move(component));
        }
        component.clear();
    }

    std::string result;
    for (const auto& part : components) {
        if (!result.empty()) result += '/';
        result += part;
    }
    return result;
}

inline bool isRoot(const std::string& path) {
    return normalized(path).empty();
}

inline bool isDescendant(const std::string& ancestor, const std::string& candidate) {
    const std::string base = normalized(ancestor);
    const std::string path = normalized(candidate);
    if (base.empty() || path.size() <= base.size() || path[base.size()] != '/') return false;
    for (size_t i = 0; i < base.size(); ++i) {
        const auto lowerAscii = [](char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        };
        if (lowerAscii(base[i]) != lowerAscii(path[i])) return false;
    }
    return true;
}

// Join base + child with exactly one separator.
inline std::string buildUrl(const std::string& baseUrl, const std::string& path) {
    if (path.empty() || path == "/") return baseUrl;
    if (baseUrl.empty()) return path;
    std::string result = baseUrl;
    if (result.back() != '/' && path.front() != '/') result += "/";
    result += path;
    return result;
}

inline std::string extractFileName(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    return (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
}

inline std::string parentOf(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    std::string parent = (lastSlash != std::string::npos && lastSlash > 0) ? path.substr(0, lastSlash) : "/";
    if (parent.empty()) parent = "/";
    return parent;
}

// Strip a "file://" prefix from a local URI.
inline std::string convertUriToPath(const std::string& path) {
    const std::string fileProtocol = "file://";
    if (path.find(fileProtocol) == 0) return path.substr(fileProtocol.length());
    return path;
}

// Same-directory sibling used for atomic download/upload commit.
// Example: /a/b/photo.jpg + task_1 → /a/b/photo.jpg.task_1.part
inline std::string tempSiblingPath(const std::string& finalPath, const std::string& taskId) {
    return finalPath + "." + taskId + ".part";
}

inline std::string backupSiblingPath(const std::string& finalPath, const std::string& taskId) {
    return finalPath + "." + taskId + ".bak";
}

// Build an SmbFileInfo from a stat struct (childCount unknown = -1).
inline SmbFileInfo makeFileInfo(const struct smb2_stat_64& stat, const std::string& name, const std::string& path) {
    bool isDirectory = (stat.smb2_type & SMB2_TYPE_DIRECTORY) != 0;
    int64_t size = isDirectory ? 0 : static_cast<int64_t>(stat.smb2_size);
    return SmbFileInfo{
        name,
        path,
        size,
        isDirectory,
        static_cast<int64_t>(stat.smb2_mtime),
        static_cast<int64_t>(stat.smb2_atime),
        static_cast<int64_t>(stat.smb2_btime),
        static_cast<int64_t>(stat.smb2_ctime),
        -1,
        {},
        std::nullopt,
    };
}

}  // namespace path_util
}  // namespace react_native_smb
