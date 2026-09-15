#include <unordered_map>
#pragma once

#include <NitroModules/HybridObject.hpp>
#include <NitroModules/JSIConverter.hpp>
#include <NitroModules/Promise.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/SmbTask.hpp"

namespace react_native_smb {
using namespace margelo::nitro;

using SmbStatusCallback = std::function<void(const std::string&, const std::string&, int)>;
using SmbProgressCallback = std::function<void(double, double)>;

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
    std::string aceType;                // Type of ACE (e.g., "ACCESS_ALLOWED_ACE",
                                        // "ACCESS_DENIED_ACE")
    std::vector<std::string> aceFlags;  // Flags for the ACE (e.g.,
                                        // "INHERIT_ONLY", "NO_PROPAGATE_INHERIT")
    std::vector<std::string> mask;      // Access mask (e.g., "READ", "WRITE", "EXECUTE")
    std::string sid;                    // Security Identifier
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
    int64_t modifiedAt;                 // Content modification time (smb2_mtime)
    int64_t accessedAt;                 // Last access time (smb2_atime)
    int64_t createdAt;                  // Creation time (smb2_btime)
    int64_t changedAt;                  // Attribute change time (smb2_ctime)
    int64_t childCount;                 // Directories: number of immediate children; -1 if unknown / not a directory
    std::vector<SmbFileInfo> children;  // Nested children for directories (when recursive)
    std::optional<SmbSecurityDescriptor> securityDescriptor;
};

class ReactNativeSmb : public virtual HybridObject {
   public:
    virtual ~ReactNativeSmb() = default;

    virtual bool isConnected() = 0;
    virtual bool isInitialized() = 0;

    // --- Connection Management ---
    virtual std::shared_ptr<SmbTask> initialize(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) = 0;
    virtual std::shared_ptr<SmbTask> connect(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) = 0;
    virtual std::shared_ptr<SmbTask> connectShare(const std::string& taskId, const std::string& share) = 0;
    virtual std::shared_ptr<SmbTask> disconnect(const std::string& taskId) = 0;
    virtual std::shared_ptr<SmbTask> listShares(const std::string& taskId) = 0;

    // --- Listing & Info ---
    virtual std::shared_ptr<SmbTask> listDirectory(const std::string& taskId, const std::string& path, bool recursive, int maxDepth, bool includeSecurityDescriptor) = 0;
    virtual std::shared_ptr<SmbTask> getPathInfo(const std::string& taskId, const std::string& path) = 0;
    virtual std::shared_ptr<SmbTask> getSecurityDescriptor(const std::string& taskId, const std::string& path) = 0;

    // --- File Transfers ---
    virtual std::shared_ptr<SmbTask> downloadFile(const std::string& taskId, const std::string& remotePath, const std::string& localPath) = 0;
    virtual std::shared_ptr<SmbTask> uploadFile(const std::string& taskId, const std::string& localPath, const std::string& remotePath) = 0;

    // --- Mutating File Operations ---
    virtual std::shared_ptr<SmbTask> createDirectory(const std::string& taskId, const std::string& path) = 0;
    virtual std::shared_ptr<SmbTask> deleteItem(const std::string& taskId, const std::string& path) = 0;
    virtual std::shared_ptr<SmbTask> moveItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath) = 0;
    virtual std::shared_ptr<SmbTask> renameItem(const std::string& taskId, const std::string& currentPath, const std::string& newName) = 0;
    virtual std::shared_ptr<SmbTask> copyItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath, bool recursive) = 0;
    virtual std::shared_ptr<SmbTask> duplicateItem(const std::string& taskId, const std::string& path) = 0;

    // --- Task control & Task Center APIs ---
    virtual std::string subscribeTaskEvents(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) = 0;
    virtual void unsubscribeTaskEvents(const std::string& subscriptionId) = 0;
    virtual std::unordered_map<std::string, std::string> getTask(const std::string& taskId) = 0;
    virtual std::vector<std::unordered_map<std::string, std::string>> getActiveTasks() = 0;
    virtual std::vector<std::unordered_map<std::string, std::string>> getTransferTasks() = 0;
    virtual std::vector<std::unordered_map<std::string, std::string>> getTaskHistory(int limit, int offset) = 0;
    virtual void cancelTask(const std::string& taskId) = 0;
    // Cancels unsettled download/upload/copy/duplicate tasks only.
    virtual void cancelTransferTasks() = 0;
    virtual void clearTaskHistory(int64_t beforeTs) = 0;

    // Tear down the client (cancel tasks, disconnect pool, wait workers). Not reusable.
    virtual std::shared_ptr<margelo::nitro::Promise<void>> destroy() = 0;

    // Debug / instrumentation
    virtual std::string subscribePoolInfo(const std::function<void(const std::vector<std::unordered_map<std::string, std::string>>&)>& listener) = 0;
    virtual void unsubscribePoolInfo(const std::string& subscriptionId) = 0;
    virtual std::vector<std::unordered_map<std::string, std::string>> getPoolInfo() = 0;
    virtual void resetPool() = 0;

   protected:
    static constexpr auto NAME = "ReactNativeSmb";
};

}  // namespace react_native_smb

namespace margelo::nitro {
using namespace react_native_smb;

namespace react_native_smb_detail {
inline jsi::Value smbSecurityDescriptorToJSI(jsi::Runtime& runtime, const SmbSecurityDescriptor& sd);
inline SmbSecurityDescriptor smbSecurityDescriptorFromJSI(jsi::Runtime& runtime, const jsi::Value& value);
}  // namespace react_native_smb_detail

template <>
struct JSIConverter<SmbCredentials> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbCredentials& credentials) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "username", jsi::String::createFromUtf8(runtime, credentials.username));
        obj.setProperty(runtime, "password", jsi::String::createFromUtf8(runtime, credentials.password));
        return obj;
    }

    static SmbCredentials fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbCredentials must be an object");
        }

        jsi::Object obj = value.asObject(runtime);
        SmbCredentials credentials;

        credentials.username = obj.getProperty(runtime, "username").asString(runtime).utf8(runtime);
        credentials.password = obj.getProperty(runtime, "password").asString(runtime).utf8(runtime);

        return credentials;
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

template <>
struct JSIConverter<SmbConnectionInfo> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbConnectionInfo& connection) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "url", jsi::String::createFromUtf8(runtime, connection.url));
        obj.setProperty(runtime, "server", jsi::String::createFromUtf8(runtime, connection.server));
        obj.setProperty(runtime, "share", jsi::String::createFromUtf8(runtime, connection.share));
        obj.setProperty(runtime, "isConnected", jsi::Value(connection.isConnected));
        return obj;
    }

    static SmbConnectionInfo fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbConnectionInfo must be an object");
        }

        jsi::Object obj = value.asObject(runtime);
        return SmbConnectionInfo{obj.getProperty(runtime, "url").asString(runtime).utf8(runtime), obj.getProperty(runtime, "server").asString(runtime).utf8(runtime),
                                 obj.getProperty(runtime, "share").asString(runtime).utf8(runtime), obj.getProperty(runtime, "isConnected").asBool()};
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

template <>
struct JSIConverter<SmbFileInfo> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbFileInfo& fileInfo) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "name", jsi::String::createFromUtf8(runtime, fileInfo.name));
        obj.setProperty(runtime, "path", jsi::String::createFromUtf8(runtime, fileInfo.path));
        obj.setProperty(runtime, "size", jsi::Value(static_cast<double>(fileInfo.size)));
        obj.setProperty(runtime, "isDirectory", jsi::Value(fileInfo.isDirectory));
        obj.setProperty(runtime, "modifiedAt", jsi::Value(static_cast<double>(fileInfo.modifiedAt)));
        obj.setProperty(runtime, "accessedAt", jsi::Value(static_cast<double>(fileInfo.accessedAt)));
        obj.setProperty(runtime, "createdAt", jsi::Value(static_cast<double>(fileInfo.createdAt)));
        obj.setProperty(runtime, "changedAt", jsi::Value(static_cast<double>(fileInfo.changedAt)));
        obj.setProperty(runtime, "childCount", jsi::Value(static_cast<double>(fileInfo.childCount)));

        jsi::Array childrenArray(runtime, fileInfo.children.size());
        for (size_t i = 0; i < fileInfo.children.size(); i++) {
            childrenArray.setValueAtIndex(runtime, i, toJSI(runtime, fileInfo.children[i]));
        }
        obj.setProperty(runtime, "children", std::move(childrenArray));

        if (fileInfo.securityDescriptor.has_value()) {
            obj.setProperty(runtime, "securityDescriptor",
                            react_native_smb_detail::smbSecurityDescriptorToJSI(runtime, *fileInfo.securityDescriptor));
        }

        return obj;
    }

    static SmbFileInfo fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbFileInfo must be an object");
        }

        jsi::Object obj = value.asObject(runtime);

        std::vector<SmbFileInfo> children;
        if (obj.hasProperty(runtime, "children")) {
            jsi::Value childrenValue = obj.getProperty(runtime, "children");
            if (childrenValue.isObject() && childrenValue.asObject(runtime).isArray(runtime)) {
                jsi::Array childrenArray = childrenValue.asObject(runtime).asArray(runtime);
                size_t childrenCount = childrenArray.size(runtime);
                children.reserve(childrenCount);

                for (size_t i = 0; i < childrenCount; i++) {
                    jsi::Value childValue = childrenArray.getValueAtIndex(runtime, i);
                    children.push_back(fromJSI(runtime, childValue));
                }
            }
        }

        std::optional<SmbSecurityDescriptor> securityDescriptor;
        if (obj.hasProperty(runtime, "securityDescriptor")) {
            jsi::Value securityDescriptorValue = obj.getProperty(runtime, "securityDescriptor");
            if (securityDescriptorValue.isObject()) {
                securityDescriptor = react_native_smb_detail::smbSecurityDescriptorFromJSI(runtime, securityDescriptorValue);
            }
        }

        return SmbFileInfo{obj.getProperty(runtime, "name").asString(runtime).utf8(runtime),
                           obj.getProperty(runtime, "path").asString(runtime).utf8(runtime),
                           static_cast<int64_t>(obj.getProperty(runtime, "size").asNumber()),
                           obj.getProperty(runtime, "isDirectory").asBool(),
                           static_cast<int64_t>(obj.getProperty(runtime, "modifiedAt").asNumber()),
                           static_cast<int64_t>(obj.hasProperty(runtime, "accessedAt") ? obj.getProperty(runtime, "accessedAt").asNumber() : 0),
                           static_cast<int64_t>(obj.hasProperty(runtime, "createdAt") ? obj.getProperty(runtime, "createdAt").asNumber() : 0),
                           static_cast<int64_t>(obj.hasProperty(runtime, "changedAt") ? obj.getProperty(runtime, "changedAt").asNumber() : 0),
                            static_cast<int64_t>(obj.hasProperty(runtime, "childCount") ? obj.getProperty(runtime, "childCount").asNumber() : -1),
                            children,
                            securityDescriptor};
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

template <>
struct JSIConverter<SmbShare> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbShare& share) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "name", jsi::String::createFromUtf8(runtime, share.name));
        obj.setProperty(runtime, "comment", jsi::String::createFromUtf8(runtime, share.comment));
        return obj;
    }

    static SmbShare fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbShare must be an object");
        }

        jsi::Object obj = value.asObject(runtime);
        return SmbShare{obj.getProperty(runtime, "name").asString(runtime).utf8(runtime), obj.getProperty(runtime, "comment").asString(runtime).utf8(runtime)};
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

template <>
struct JSIConverter<SmbAce> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbAce& ace) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "aceType", jsi::String::createFromUtf8(runtime, ace.aceType));

        jsi::Array aceFlagsArray = jsi::Array(runtime, ace.aceFlags.size());
        for (size_t i = 0; i < ace.aceFlags.size(); i++) {
            aceFlagsArray.setValueAtIndex(runtime, i, jsi::String::createFromUtf8(runtime, ace.aceFlags[i]));
        }
        obj.setProperty(runtime, "aceFlags", aceFlagsArray);

        jsi::Array maskArray = jsi::Array(runtime, ace.mask.size());
        for (size_t i = 0; i < ace.mask.size(); i++) {
            maskArray.setValueAtIndex(runtime, i, jsi::String::createFromUtf8(runtime, ace.mask[i]));
        }
        obj.setProperty(runtime, "mask", maskArray);

        obj.setProperty(runtime, "sid", jsi::String::createFromUtf8(runtime, ace.sid));
        return obj;
    }

    static SmbAce fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbAce must be an object");
        }

        jsi::Object obj = value.asObject(runtime);
        SmbAce ace;

        ace.aceType = obj.getProperty(runtime, "aceType").asString(runtime).utf8(runtime);

        if (obj.hasProperty(runtime, "aceFlags")) {
            jsi::Value aceFlagsValue = obj.getProperty(runtime, "aceFlags");
            if (aceFlagsValue.isObject() && aceFlagsValue.asObject(runtime).isArray(runtime)) {
                jsi::Array aceFlagsArray = aceFlagsValue.asObject(runtime).asArray(runtime);
                size_t flagsCount = aceFlagsArray.size(runtime);
                ace.aceFlags.reserve(flagsCount);

                for (size_t i = 0; i < flagsCount; i++) {
                    jsi::Value flagItem = aceFlagsArray.getValueAtIndex(runtime, i);
                    if (flagItem.isString()) {
                        ace.aceFlags.push_back(flagItem.asString(runtime).utf8(runtime));
                    }
                }
            }
        }

        if (obj.hasProperty(runtime, "mask")) {
            jsi::Value maskValue = obj.getProperty(runtime, "mask");
            if (maskValue.isObject() && maskValue.asObject(runtime).isArray(runtime)) {
                jsi::Array maskArray = maskValue.asObject(runtime).asArray(runtime);
                size_t maskCount = maskArray.size(runtime);
                ace.mask.reserve(maskCount);

                for (size_t i = 0; i < maskCount; i++) {
                    jsi::Value maskItem = maskArray.getValueAtIndex(runtime, i);
                    if (maskItem.isString()) {
                        ace.mask.push_back(maskItem.asString(runtime).utf8(runtime));
                    }
                }
            }
        }

        ace.sid = obj.getProperty(runtime, "sid").asString(runtime).utf8(runtime);

        return ace;
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

template <>
struct JSIConverter<SmbAcl> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbAcl& acl) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "revision", jsi::Value(static_cast<double>(acl.revision)));
        obj.setProperty(runtime, "aceCount", jsi::Value(static_cast<double>(acl.aceCount)));

        jsi::Array aces = jsi::Array(runtime, acl.aces.size());
        for (size_t i = 0; i < acl.aces.size(); i++) {
            aces.setValueAtIndex(runtime, i, JSIConverter<SmbAce>::toJSI(runtime, acl.aces[i]));
        }
        obj.setProperty(runtime, "aces", aces);

        return obj;
    }

    static SmbAcl fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbAcl must be an object");
        }

        jsi::Object obj = value.asObject(runtime);
        SmbAcl acl;
        acl.revision = static_cast<uint8_t>(obj.getProperty(runtime, "revision").asNumber());
        acl.aceCount = static_cast<uint16_t>(obj.getProperty(runtime, "aceCount").asNumber());

        if (obj.hasProperty(runtime, "aces")) {
            jsi::Array aces = obj.getProperty(runtime, "aces").asObject(runtime).asArray(runtime);
            size_t aceCount = aces.size(runtime);
            acl.aces.reserve(aceCount);

            for (size_t i = 0; i < aceCount; i++) {
                jsi::Value aceValue = aces.getValueAtIndex(runtime, i);
                acl.aces.push_back(JSIConverter<SmbAce>::fromJSI(runtime, aceValue));
            }
        }

        return acl;
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

template <>
struct JSIConverter<SmbSecurityDescriptor> {
    static jsi::Value toJSI(jsi::Runtime& runtime, const SmbSecurityDescriptor& sd) {
        jsi::Object obj(runtime);
        obj.setProperty(runtime, "revision", jsi::Value(static_cast<double>(sd.revision)));

        jsi::Array controlArray = jsi::Array(runtime, sd.control.size());
        for (size_t i = 0; i < sd.control.size(); i++) {
            controlArray.setValueAtIndex(runtime, i, jsi::String::createFromUtf8(runtime, sd.control[i]));
        }
        obj.setProperty(runtime, "control", controlArray);

        obj.setProperty(runtime, "ownerSid", jsi::String::createFromUtf8(runtime, sd.ownerSid));
        obj.setProperty(runtime, "groupSid", jsi::String::createFromUtf8(runtime, sd.groupSid));
        obj.setProperty(runtime, "dacl", JSIConverter<SmbAcl>::toJSI(runtime, sd.dacl));
        return obj;
    }

    static SmbSecurityDescriptor fromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
        if (!value.isObject()) {
            throw std::invalid_argument("SmbSecurityDescriptor must be an object");
        }

        jsi::Object obj = value.asObject(runtime);
        SmbSecurityDescriptor sd;

        sd.revision = static_cast<uint8_t>(obj.getProperty(runtime, "revision").asNumber());

        if (obj.hasProperty(runtime, "control")) {
            jsi::Value controlValue = obj.getProperty(runtime, "control");
            if (controlValue.isObject() && controlValue.asObject(runtime).isArray(runtime)) {
                jsi::Array controlArray = controlValue.asObject(runtime).asArray(runtime);
                size_t controlCount = controlArray.size(runtime);
                sd.control.reserve(controlCount);

                for (size_t i = 0; i < controlCount; i++) {
                    jsi::Value controlItem = controlArray.getValueAtIndex(runtime, i);
                    if (controlItem.isString()) {
                        sd.control.push_back(controlItem.asString(runtime).utf8(runtime));
                    }
                }
            }
        }

        sd.ownerSid = obj.getProperty(runtime, "ownerSid").asString(runtime).utf8(runtime);
        sd.groupSid = obj.getProperty(runtime, "groupSid").asString(runtime).utf8(runtime);
        sd.dacl = JSIConverter<SmbAcl>::fromJSI(runtime, obj.getProperty(runtime, "dacl"));

        return sd;
    }

    static bool canConvert(jsi::Runtime& runtime, const jsi::Value& value) { return value.isObject(); }
};

namespace react_native_smb_detail {
inline jsi::Value smbSecurityDescriptorToJSI(jsi::Runtime& runtime, const SmbSecurityDescriptor& sd) {
    return JSIConverter<SmbSecurityDescriptor>::toJSI(runtime, sd);
}

inline SmbSecurityDescriptor smbSecurityDescriptorFromJSI(jsi::Runtime& runtime, const jsi::Value& value) {
    return JSIConverter<SmbSecurityDescriptor>::fromJSI(runtime, value);
}
}  // namespace react_native_smb_detail

}  // namespace margelo::nitro
