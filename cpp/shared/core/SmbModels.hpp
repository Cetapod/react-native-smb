#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace react_native_smb {

struct SmbCredentials {
    std::string username;
    std::string password;
};

struct SmbShare {
    std::string name;
    std::string comment;
};

struct SmbConnectionInfo {
    std::string url;
    std::string server;
    std::string share;
    bool isConnected;
};

struct SmbAce {
    std::string aceType;
    std::vector<std::string> aceFlags;
    std::vector<std::string> mask;
    std::string sid;
};

struct SmbAcl {
    uint8_t revision;
    uint16_t aceCount;
    std::vector<SmbAce> aces;
};

struct SmbSecurityDescriptor {
    uint8_t revision;
    std::vector<std::string> control;
    std::string ownerSid;
    std::string groupSid;
    SmbAcl dacl;
};

struct SmbFileInfo {
    std::string name;
    std::string path;
    int64_t size;
    bool isDirectory;
    int64_t modifiedAt;
    int64_t accessedAt;
    int64_t createdAt;
    int64_t changedAt;
    int64_t childCount;  // -1 if unknown / not a directory
    std::vector<SmbFileInfo> children;
    std::optional<SmbSecurityDescriptor> securityDescriptor;
};

}  // namespace react_native_smb
