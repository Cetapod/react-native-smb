#pragma once

#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <cstdint>
#include <string>

#include "../ReactNativeSmb.hpp"  // SmbFileInfo

namespace react_native_smb {

// Path and stat helpers shared by operators.
namespace path_util {

// Strip a single leading '/'; map "." / "/" / "" to empty (libsmb2 root form).
inline std::string normalized(const std::string& path) {
    std::string p = path;
    if (p == "." || p == "/" || p.empty()) {
        p = "";
    } else if (p[0] == '/') {
        p = p.substr(1);
    }
    return p;
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
    };
}

}  // namespace path_util
}  // namespace react_native_smb
