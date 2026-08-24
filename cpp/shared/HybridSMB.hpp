#pragma once

#include "ReactNativeSmb.hpp"
#include "connection/SmbConnectionPool.hpp"
#include "core/SmbTask.hpp"
#include "core/TaskObserverHub.hpp"
#include "operators/OperatorBase.hpp"

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <NitroModules/HybridObject.hpp>
#include <NitroModules/Promise.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace react_native_smb {
using namespace margelo::nitro;

class HybridSMB : public ReactNativeSmb {
   private:
    std::unique_ptr<SmbConnectionPool> pool_;
    // Optional observer; tracks every task this instance creates.
    std::unique_ptr<TaskObserverHub> observer_;

    // Live tasks, kept only so cancelTask/getTask can locate them.
    std::map<std::string, std::shared_ptr<SmbTask>> tasks_;
    std::mutex tasksMutex_;

    using PoolListener = std::function<void(const std::vector<std::unordered_map<std::string, std::string>>&)>;
    std::map<std::string, PoolListener> poolListeners_;
    std::map<std::string, size_t> poolObserverIds_;
    std::mutex poolListenersMutex_;
    std::atomic<size_t> poolListenerToken_{1};

    // SmbTask factory
    template <typename OpT, typename R>
    std::shared_ptr<SmbTask> createTask(const std::string& taskId, std::unique_ptr<OpT> seedOp);


   public:
    HybridSMB();
    virtual ~HybridSMB();

    // Implement ReactNativeSmb interface

    // --- State Checks ---
    bool isConnected() override;
    bool isInitialized() override;

    // --- Connection Management ---
    std::shared_ptr<SmbTask> initialize(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) override;
    std::shared_ptr<SmbTask> connect(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) override;
    std::shared_ptr<SmbTask> connectShare(const std::string& taskId, const std::string& share) override;
    std::shared_ptr<SmbTask> disconnect(const std::string& taskId) override;
    std::shared_ptr<SmbTask> listShares(const std::string& taskId) override;

    // --- Listing & Info ---
    std::shared_ptr<SmbTask> listDirectory(const std::string& taskId, const std::string& path, bool recursive, int maxDepth, bool includeSecurityDescriptor) override;
    std::shared_ptr<SmbTask> getPathInfo(const std::string& taskId, const std::string& path) override;
    std::shared_ptr<SmbTask> getSecurityDescriptor(const std::string& taskId, const std::string& path) override;

    // --- File Transfers ---
    std::shared_ptr<SmbTask> downloadFile(const std::string& taskId, const std::string& remotePath, const std::string& localPath) override;
    std::shared_ptr<SmbTask> uploadFile(const std::string& taskId, const std::string& localPath, const std::string& remotePath) override;

    // --- Mutating File Operations ---
    std::shared_ptr<SmbTask> createDirectory(const std::string& taskId, const std::string& path) override;
    std::shared_ptr<SmbTask> deleteItem(const std::string& taskId, const std::string& path) override;
    std::shared_ptr<SmbTask> moveItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath) override;
    std::shared_ptr<SmbTask> renameItem(const std::string& taskId, const std::string& currentPath, const std::string& newName) override;
    std::shared_ptr<SmbTask> copyItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath, bool recursive) override;
    std::shared_ptr<SmbTask> duplicateItem(const std::string& taskId, const std::string& path) override;

    // Task control
    std::string subscribeTaskEvents(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) override;
    void unsubscribeTaskEvents(const std::string& subscriptionId) override;
    std::unordered_map<std::string, std::string> getTask(const std::string& taskId) override;
    std::vector<std::unordered_map<std::string, std::string>> getActiveTasks() override;
    std::vector<std::unordered_map<std::string, std::string>> getTaskHistory(int limit, int offset) override;
    void cancelTask(const std::string& taskId) override;
    void clearTaskHistory(int64_t beforeTs) override;

    // Debug / instrumentation
    std::string subscribePoolInfo(const std::function<void(const std::vector<std::unordered_map<std::string, std::string>>&)>& listener) override;
    void unsubscribePoolInfo(const std::string& subscriptionId) override;
    std::vector<std::unordered_map<std::string, std::string>> getPoolInfo() override;
    void resetPool() override;

   protected:
    void loadHybridMethods() override;
};

}  // namespace react_native_smb
