#pragma once

#include "HybridNativeSmbListTaskSpec.hpp"
#include "HybridReactNativeSmbSpec.hpp"
#include "HybridSmbListTask.hpp"
#include "HybridSmbTask.hpp"

#include "../connection/SmbConnectionPool.hpp"
#include "../core/LifecycleSerialExecutor.hpp"
#include "../core/SmbTask.hpp"
#include "../core/TaskObserverHub.hpp"
#include "../operators/OperatorBase.hpp"

#include <NitroModules/Promise.hpp>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace margelo::nitro::smb {

class HybridSMB : public HybridReactNativeSmbSpec {
   private:
    std::shared_ptr<react_native_smb::SmbConnectionPool> pool_;
    std::unique_ptr<react_native_smb::TaskObserverHub> observer_;

    std::map<std::string, std::shared_ptr<react_native_smb::SmbTask>> tasks_;
    std::mutex tasksMutex_;
    struct TaskRunState {
        std::mutex mutex;
        std::condition_variable cv;
        size_t active{0};
    };
    std::shared_ptr<TaskRunState> taskRuns_{std::make_shared<TaskRunState>()};
    react_native_smb::LifecycleSerialExecutor lifecycleExecutor_;
    std::atomic<bool> disposed_{false};
    std::mutex shutdownMutex_;

    void cancelAllTasksExcept(const std::string& taskId);
    void cancelTasksMatching(const std::function<bool(const std::shared_ptr<react_native_smb::SmbTask>&)>& pred);
    void forgetTask(const std::string& taskId);
    void shutdownImpl();
    static void releaseTaskRun(const std::shared_ptr<TaskRunState>& state);

    using PoolListener = std::function<void(const std::vector<std::unordered_map<std::string, std::string>>&)>;
    std::map<std::string, PoolListener> poolListeners_;
    std::map<std::string, size_t> poolObserverIds_;
    std::mutex poolListenersMutex_;
    std::atomic<size_t> poolListenerToken_{1};

    enum class ExecutionLane { Shared, Lifecycle };

    template <typename OpT>
    std::shared_ptr<react_native_smb::SmbTask> startTask(const std::string& taskId, std::unique_ptr<OpT> seedOp,
                                                         ExecutionLane lane = ExecutionLane::Shared);

    template <typename OpT>
    std::shared_ptr<HybridNativeSmbTaskSpec> createTask(const std::string& taskId, std::unique_ptr<OpT> seedOp,
                                                        ExecutionLane lane = ExecutionLane::Shared);

   public:
    HybridSMB();
    ~HybridSMB() override;

    // --- state ---
    bool isConnected() override;
    bool isInitialized() override;

    // --- connection ---
    std::shared_ptr<HybridNativeSmbTaskSpec> initialize(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> connect(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> connectShare(const std::string& taskId, const std::string& share) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> disconnect(const std::string& taskId) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> listShares(const std::string& taskId) override;

    // --- listing ---
    std::shared_ptr<HybridNativeSmbListTaskSpec> listDirectory(const std::string& taskId, const std::string& path, double maxDepth, bool includeSecurityDescriptor, double batchSize) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> getPathInfo(const std::string& taskId, const std::string& path) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> getSecurityDescriptor(const std::string& taskId, const std::string& path) override;

    // --- transfer ---
    std::shared_ptr<HybridNativeSmbTaskSpec> downloadFile(const std::string& taskId, const std::string& remotePath, const std::string& localPath) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> uploadFile(const std::string& taskId, const std::string& localPath, const std::string& remotePath) override;

    // --- mutate ---
    std::shared_ptr<HybridNativeSmbTaskSpec> createDirectory(const std::string& taskId, const std::string& path) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> deleteItem(const std::string& taskId, const std::string& path) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> moveItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> renameItem(const std::string& taskId, const std::string& currentPath, const std::string& newName) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> copyItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath, bool recursive) override;
    std::shared_ptr<HybridNativeSmbTaskSpec> duplicateItem(const std::string& taskId, const std::string& path) override;

    // --- tasks ---
    std::string subscribeTaskEvents(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) override;
    void unsubscribeTaskEvents(const std::string& subscriptionId) override;
    std::unordered_map<std::string, std::string> getTask(const std::string& taskId) override;
    std::vector<std::unordered_map<std::string, std::string>> getActiveTasks() override;
    std::vector<std::unordered_map<std::string, std::string>> getTransferTasks() override;
    std::vector<std::unordered_map<std::string, std::string>> getTaskHistory(double limit, double offset) override;
    void cancelTask(const std::string& taskId) override;
    void cancelTransferTasks() override;
    void clearTaskHistory(int64_t beforeTs) override;

    // --- lifecycle ---
    std::shared_ptr<Promise<void>> destroy() override;

    // --- pool ---
    std::string subscribePoolInfo(const std::function<void(const std::vector<std::unordered_map<std::string, std::string>>&)>& listener) override;
    void unsubscribePoolInfo(const std::string& subscriptionId) override;
    std::vector<std::unordered_map<std::string, std::string>> getPoolInfo() override;
    void resetPool() override;
};

}  // namespace margelo::nitro::smb
