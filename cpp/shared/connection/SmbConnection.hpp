#pragma once

#include "../ReactNativeSmb.hpp"
#include "../util/SmbErrorMapper.hpp"

#include <cstdint>
#include <cstddef>
#include <ctime>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

namespace react_native_smb {
class SmbConnectionManager;

// RAII lock over connectionMutex_; auto-reconnects on construction.
class ScopedContextLock {
   private:
    std::unique_lock<std::recursive_timed_mutex> lock_;
    smb2_context* context_;

   public:
    ScopedContextLock(SmbConnectionManager& manager, const std::string& taskId = "");

    smb2_context* get() const { return context_; }

    ScopedContextLock(const ScopedContextLock&) = delete;
    ScopedContextLock& operator=(const ScopedContextLock&) = delete;
};

// Per-manager recursive_timed_mutex serialises all smb2_* calls on one context.
// submitSync() acquires the mutex and runs the callable on the calling thread.
// No background poll thread — a second poller on the same fd corrupts libsmb2.
class SmbConnectionManager {
    friend class ScopedContextLock;

   private:
    std::shared_ptr<smb2_context> context_;
    std::shared_ptr<SmbCredentials> credentials_;
    std::recursive_timed_mutex connectionMutex_;
    std::string currentUrl_;
    std::string serverName_;
    std::string currentShareName_;
    bool isInitialized_ = false;
    bool isConnected_ = false;

   public:
    SmbConnectionManager();
    ~SmbConnectionManager();

    void initialize(const std::string& url, const SmbCredentials& credentials, const std::string& taskId = "");
    void connect(const std::string& url, const SmbCredentials& credentials, const std::string& taskId = "");
    void connectShare(const std::string& share, const std::string& taskId = "");
    void disconnect(const std::string& taskId = "");

    std::vector<SmbShare> listShares(const std::string& taskId = "");

    void checkAndInitialize(const std::string& taskId = "");
    void checkAndConnect(const std::string& taskId = "");
    void invalidateContext();

    bool isConnected() const { return isConnected_; }
    bool isInitialized() const { return isInitialized_; }
    const std::string& getCurrentUrl() const { return currentUrl_; }
    const std::string& getServerName() const { return serverName_; }
    const std::string& getShareName() const { return currentShareName_; }

    std::shared_ptr<smb2_context> getContext() { return context_; }
    const std::shared_ptr<SmbCredentials>& getCredentials() const { return credentials_; }

    std::recursive_timed_mutex& getConnectionMutex() { return connectionMutex_; }

    smb2_context* rawCtx() const { return context_.get(); }

    template <typename Fn, typename R = std::invoke_result_t<Fn, smb2_context*>>
    R submitSync(Fn&& fn) {
        std::lock_guard<std::recursive_timed_mutex> lock(connectionMutex_);
        if constexpr (std::is_void_v<R>) {
            fn(context_.get());
        } else {
            return fn(context_.get());
        }
    }

   private:
    void initializeContext();
};
}  // namespace react_native_smb
