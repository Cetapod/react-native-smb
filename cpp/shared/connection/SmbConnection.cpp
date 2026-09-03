#include "SmbConnection.hpp"  // sibling

#include <poll.h>
#include <smb2/libsmb2-dcerpc-srvsvc.h>
#include <smb2/libsmb2-raw.h>

#include <cerrno>
#include <memory>
#include <stdexcept>

#include "../util/SmbException.hpp"

namespace react_native_smb {

namespace {
constexpr int kShareEnumPollTimeoutMs = 5000;
constexpr int kShareEnumMaxIdlePolls = 12;

struct ShareEnumContext {
    std::vector<SmbShareList>* shares;
    std::string error;
    int errorCode;
    bool finished;
};

[[noreturn]] void throwError(const std::string& /*taskId*/, const std::string& message, int code) {
    SmbException::raise(code, message);
}

[[noreturn]] void rethrowOrUnknown(const std::string& taskId, const std::exception& e) {
    if (dynamic_cast<const SmbException*>(&e) != nullptr) throw;
    throwError(taskId, e.what(), static_cast<int>(SmbErrorCode::Unknown));
}
}  // namespace

ScopedContextLock::ScopedContextLock(SmbConnectionManager& manager, const std::string& taskId) : lock_(manager.connectionMutex_), context_(nullptr) {
    manager.checkAndConnect(taskId);
    context_ = manager.context_.get();
}

SmbConnectionManager::SmbConnectionManager() {
    context_ = nullptr;
    currentUrl_ = "";
    serverName_ = "";
    isInitialized_ = false;
    isConnected_ = false;
}

SmbConnectionManager::~SmbConnectionManager() {
    std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);
    if (isConnected_ && context_) {
        smb2_disconnect_share(context_.get());
        isConnected_ = false;
        currentShareName_.clear();
    }
    context_.reset();
}

void SmbConnectionManager::initialize(const std::string& url, const SmbCredentials& credentials, const std::string& taskId) {
    try {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);

        initializeContext();

        credentials_ = std::make_shared<SmbCredentials>(credentials);

        smb2_set_user(context_.get(), credentials.username.c_str());
        smb2_set_password(context_.get(), credentials.password.c_str());

        if (url.substr(0, 6) == "smb://") {
            std::string remaining = url.substr(6);
            size_t slashPos = remaining.find('/');
            if (slashPos != std::string::npos) {
                serverName_ = remaining.substr(0, slashPos);
            } else {
                serverName_ = remaining;
            }
        } else {
            std::string msg = "Invalid SMB URL format. Expected: smb://server";
            throwError(taskId, msg, static_cast<int>(SmbErrorCode::InvalidArgument));
        }

        currentUrl_ = "smb://" + serverName_;
        isInitialized_ = true;
    } catch (const std::exception& e) {
        rethrowOrUnknown(taskId, e);
    }
}

void SmbConnectionManager::connect(const std::string& url, const SmbCredentials& credentials, const std::string& taskId) {
    try {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);

        if (isConnected_) {
            smb2_disconnect_share(context_.get());
            isConnected_ = false;
        }

        initializeContext();

        credentials_ = std::make_shared<SmbCredentials>(credentials);

        smb2_set_user(context_.get(), credentials.username.c_str());
        smb2_set_password(context_.get(), credentials.password.c_str());

        std::string serverName;
        std::string shareName;

        if (url.substr(0, 6) == "smb://") {
            std::string remaining = url.substr(6);
            size_t slashPos = remaining.find('/');
            if (slashPos != std::string::npos) {
                serverName = remaining.substr(0, slashPos);
                shareName = remaining.substr(slashPos + 1);
            } else {
                std::string msg = "Invalid URL: format must be smb://server/share. Received: " + url;
                throwError(taskId, msg, static_cast<int>(SmbErrorCode::InvalidArgument));
            }
        } else {
            std::string msg = "Invalid URL: format must be smb://server/share. Received: " + url;
            throwError(taskId, msg, static_cast<int>(SmbErrorCode::InvalidArgument));
        }

        const int connectResult = smb2_connect_share(context_.get(), serverName.c_str(), shareName.c_str(), credentials.username.c_str());
        if (connectResult < 0) {
            std::string libError = smb2_get_error(context_.get());
            std::string msg = "SMB Connection Failed: Could not connect to share '" + shareName + "'. Error: " + libError;
            throwError(taskId, msg, SmbErrorMapper::fromErrnoResult(connectResult));
        }

        isConnected_ = true;
        isInitialized_ = true;
        serverName_ = serverName;
        currentShareName_ = shareName;
        currentUrl_ = url;
    } catch (const std::exception& e) {
        rethrowOrUnknown(taskId, e);
    }
}

void SmbConnectionManager::connectShare(const std::string& share, const std::string& taskId) {
    try {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);

        if (!isInitialized_) {
            std::string msg = "Connection not initialized. Call initialize() first.";
            throwError(taskId, msg, static_cast<int>(SmbErrorCode::NotConnected));
        }

        if (isConnected_ && currentShareName_ == share) {
            return;
        }

        if (isConnected_) {
            smb2_disconnect_share(context_.get());
            isConnected_ = false;
            currentShareName_.clear();
        }

        const int connectResult = smb2_connect_share(context_.get(), serverName_.c_str(), share.c_str(), credentials_->username.c_str());
        if (connectResult < 0) {
            std::string msg = "Failed to connect to SMB share '" + share + "': " + std::string(smb2_get_error(context_.get()));
            throwError(taskId, msg, SmbErrorMapper::fromErrnoResult(connectResult));
        }

        isConnected_ = true;
        currentShareName_ = share;

        currentUrl_ = "smb://" + serverName_ + "/" + share;
    } catch (const std::exception& e) {
        rethrowOrUnknown(taskId, e);
    }
}

void SmbConnectionManager::disconnect(const std::string& taskId) {
    try {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);
        if (isConnected_ && context_) {
            smb2_disconnect_share(context_.get());
            isConnected_ = false;
        }
        context_.reset();
        isInitialized_ = false;
        credentials_.reset();
        serverName_.clear();
        currentShareName_.clear();
        currentUrl_.clear();
    } catch (const std::exception& e) {
        rethrowOrUnknown(taskId, e);
    }
}

std::vector<SmbShareList> SmbConnectionManager::listShares(const std::string& taskId) {
    std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);

    if (!isInitialized_) {
        throwError(taskId, "Share List Failed: SMB context not initialized. Please call initialize() first.", static_cast<int>(SmbErrorCode::NotConnected));
    }

    if (serverName_.empty()) {
        throwError(taskId, "Share List Failed: No server name found. You must connect to a server before listing shares.", static_cast<int>(SmbErrorCode::NotConnected));
    }

    std::vector<SmbShareList> shares;
    auto enumContext = std::make_unique<ShareEnumContext>(ShareEnumContext{&shares, "", static_cast<int>(SmbErrorCode::Unknown), false});

    auto ipcContext = std::shared_ptr<smb2_context>(smb2_init_context(), [](smb2_context* ctx) {
        if (ctx) {
            smb2_destroy_context(ctx);
        }
    });

    if (!ipcContext) {
        throwError(taskId, "Share List Failed: Could not create temporary SMB context. System might be low on memory.", static_cast<int>(SmbErrorCode::Io));
    }

    if (credentials_) {
        smb2_set_user(ipcContext.get(), credentials_->username.c_str());
        smb2_set_password(ipcContext.get(), credentials_->password.c_str());
    } else {
        throwError(taskId, "No credentials available for share enumeration", static_cast<int>(SmbErrorCode::NotConnected));
    }

    const int ipcConnectResult = smb2_connect_share(ipcContext.get(), serverName_.c_str(), "IPC$", nullptr);
    if (ipcConnectResult < 0) {
        throwError(taskId, "Failed to connect to IPC$ share: " + std::string(smb2_get_error(ipcContext.get())), SmbErrorMapper::fromErrnoResult(ipcConnectResult));
    }

    auto shareEnumCallback = [](struct smb2_context* smb2, int status, void* command_data, void* private_data) {
        ShareEnumContext* ctx = static_cast<ShareEnumContext*>(private_data);
        ctx->finished = true;

        if (status != 0) {
            ctx->error = "Failed to enumerate shares: " + std::string(smb2_get_error(smb2));
            ctx->errorCode = SmbErrorMapper::fromErrnoResult(status);
            return;
        }

        auto rep = static_cast<struct srvsvc_NetrShareEnum_rep*>(command_data);
        if (!rep) {
            ctx->error = "No share enumeration data received";
            return;
        }

        for (uint32_t i = 0; i < rep->ses.ShareInfo.Level1.EntriesRead; i++) {
            auto& shareInfo = rep->ses.ShareInfo.Level1.Buffer->share_info_1[i];

            SmbShareList share;
            share.name = shareInfo.netname.utf8 ? shareInfo.netname.utf8 : "";
            share.comment = shareInfo.remark.utf8 ? shareInfo.remark.utf8 : "";

            bool isHidden = (shareInfo.type & SHARE_TYPE_HIDDEN) != 0;
            bool isSystem = (shareInfo.type & 3) == SHARE_TYPE_IPC;

            if (!isHidden && !isSystem && !share.name.empty()) {
                ctx->shares->push_back(share);
            }
        }

        smb2_free_data(smb2, rep);
    };

    const int enumStartResult = smb2_share_enum_async(ipcContext.get(), SHARE_INFO_1, shareEnumCallback, enumContext.get());
    if (enumStartResult != 0) {
        throwError(taskId, "Failed to start share enumeration: " + std::string(smb2_get_error(ipcContext.get())), SmbErrorMapper::fromErrnoResult(enumStartResult));
    }

    struct pollfd pfd;
    int idlePolls = 0;
    while (!enumContext->finished) {
        pfd.fd = smb2_get_fd(ipcContext.get());
        pfd.events = smb2_which_events(ipcContext.get());

        if (poll(&pfd, 1, kShareEnumPollTimeoutMs) < 0) {
            throwError(taskId, "Poll failed during share enumeration", SmbErrorMapper::fromErrno(errno));
        }

        if (pfd.revents == 0) {
            if (++idlePolls >= kShareEnumMaxIdlePolls) {
                throwError(taskId, "Share enumeration timed out", static_cast<int>(SmbErrorCode::TimedOut));
            }
            continue;
        }
        idlePolls = 0;

        const int serviceResult = smb2_service(ipcContext.get(), pfd.revents);
        if (serviceResult < 0) {
            throwError(taskId, "SMB2 service failed: " + std::string(smb2_get_error(ipcContext.get())), SmbErrorMapper::fromErrnoResult(serviceResult));
        }
    }

    if (!enumContext->error.empty()) {
        throwError(taskId, enumContext->error, enumContext->errorCode);
    }

    smb2_disconnect_share(ipcContext.get());

    return shares;
}

void SmbConnectionManager::checkAndInitialize(const std::string& taskId) {
    try {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);

        if (isInitialized_ && context_) {
            return;
        }

        if (!credentials_) {
            std::string msg = "SMB Reconnect Error: Missing saved credentials. Please call initialize() or connect() first.";
            throwError(taskId, msg, static_cast<int>(SmbErrorCode::NotConnected));
        }

        if (serverName_.empty()) {
            if (!currentUrl_.empty() && currentUrl_.rfind("smb://", 0) == 0) {
                std::string remaining = currentUrl_.substr(6);
                size_t slashPos = remaining.find('/');
                serverName_ = (slashPos == std::string::npos) ? remaining : remaining.substr(0, slashPos);
            }
        }

        if (serverName_.empty()) {
            std::string msg = "SMB Reconnect Error: Missing saved server name. Please call initialize() or connect() first.";
            throwError(taskId, msg, static_cast<int>(SmbErrorCode::NotConnected));
        }

        initializeContext();

        smb2_set_user(context_.get(), credentials_->username.c_str());
        smb2_set_password(context_.get(), credentials_->password.c_str());

        if (currentUrl_.empty()) {
            currentUrl_ = "smb://" + serverName_;
        }

        isInitialized_ = true;
    } catch (const std::exception& e) {
        rethrowOrUnknown(taskId, e);
    }
}

void SmbConnectionManager::checkAndConnect(const std::string& taskId) {
    try {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);

        if (isConnected_ && context_) {
            return;
        }

        checkAndInitialize(taskId);

        if (currentShareName_.empty() && currentUrl_.rfind("smb://", 0) == 0) {
            std::string remaining = currentUrl_.substr(6);
            size_t slashPos = remaining.find('/');
            if (slashPos != std::string::npos && slashPos + 1 < remaining.size()) {
                currentShareName_ = remaining.substr(slashPos + 1);
            }
        }

        if (currentShareName_.empty()) {
            std::string msg = "SMB Reconnect Error: Missing saved share name. Please call connect() or connectShare() first.";
            throwError(taskId, msg, static_cast<int>(SmbErrorCode::NotConnected));
        }

        const int connectResult = smb2_connect_share(context_.get(), serverName_.c_str(), currentShareName_.c_str(), credentials_->username.c_str());
        if (connectResult < 0) {
            std::string msg = "SMB Reconnect Failed: Could not connect to share '" + currentShareName_ + "'. Error: " + std::string(smb2_get_error(context_.get()));
            throwError(taskId, msg, SmbErrorMapper::fromErrnoResult(connectResult));
        }
        isConnected_ = true;
        currentUrl_ = "smb://" + serverName_ + "/" + currentShareName_;

    } catch (const std::exception& e) {
        rethrowOrUnknown(taskId, e);
    }
}

void SmbConnectionManager::invalidateContext() {
    std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);
    context_.reset();
    isConnected_ = false;
    isInitialized_ = false;
}

void SmbConnectionManager::initializeContext() {
    if (context_) {
        context_.reset();
    }

    auto* raw_context = smb2_init_context();
    if (!raw_context) {
        SmbException::raise(SmbErrorCode::Io, "Fatal Error: Failed to initialize libsmb2 context. System may be out of memory.");
    }

    context_ = std::shared_ptr<smb2_context>(raw_context, [](smb2_context* ctx) {
        if (ctx) {
            smb2_destroy_context(ctx);
        }
    });
}
}  // namespace react_native_smb
