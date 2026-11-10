#include "HybridSMB.hpp"

#include "BridgeMapping.hpp"

#include <NitroModules/Promise.hpp>
#include <stdexcept>
#include <utility>

#include "../connection/PoolTypes.hpp"
#include "../core/TaskSnapshotCodec.hpp"
#include "../operators/connection/ConnectOperator.hpp"
#include "../operators/connection/ConnectShareOperator.hpp"
#include "../operators/connection/DisconnectOperator.hpp"
#include "../operators/connection/InitializeOperator.hpp"
#include "../operators/connection/ListSharesOperator.hpp"
#include "../operators/listing/GetPathInfoOperator.hpp"
#include "../operators/listing/GetSecurityDescriptorOperator.hpp"
#include "../operators/listing/ListDirectoryOperator.hpp"
#include "../operators/mutate/CopyItemOperator.hpp"
#include "../operators/mutate/CreateDirectoryOperator.hpp"
#include "../operators/mutate/DeleteItemOperator.hpp"
#include "../operators/mutate/DuplicateItemOperator.hpp"
#include "../operators/mutate/MoveItemOperator.hpp"
#include "../operators/mutate/RenameItemOperator.hpp"
#include "../operators/transfer/DownloadFileOperator.hpp"
#include "../operators/transfer/UploadFileOperator.hpp"
#include "../util/SmbException.hpp"
#include "../util/SmbLog.hpp"

namespace margelo::nitro::smb {

using react_native_smb::SmbTask;
using react_native_smb::SmbTaskState;
using react_native_smb::TaskSnapshotCodec;
using react_native_smb::isTransferKind;
using react_native_smb::PoolSlotInfo;
using react_native_smb::SmbErrorCode;
using react_native_smb::SmbException;

void HybridSMB::releaseTaskRun(const std::shared_ptr<TaskRunState>& state) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        --state->active;
    }
    state->cv.notify_all();
}

template <typename OpT>
std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::createTask(const std::string& taskId, std::unique_ptr<OpT> seedOp,
                                                               ExecutionLane lane) {
    std::lock_guard<std::mutex> shutdownLock(shutdownMutex_);
    if (disposed_.load(std::memory_order_acquire)) {
        SmbException::raise(SmbErrorCode::NotConnected, "SMB client has been destroyed");
    }

    auto task = std::make_shared<SmbTask>(taskId, std::move(seedOp), pool_);
    if (observer_) {
        observer_->track(task);
    }
    {
        std::lock_guard<std::mutex> lk(tasksMutex_);
        tasks_[taskId] = task;
    }

    auto taskRuns = taskRuns_;
    {
        std::lock_guard<std::mutex> lock(taskRuns->mutex);
        ++taskRuns->active;
    }

    auto* self = this;
    auto runBody = [self, task, taskId, taskRuns] {
        struct RunCompletionGuard {
            std::shared_ptr<TaskRunState> state;
            ~RunCompletionGuard() { HybridSMB::releaseTaskRun(state); }
        } guard{taskRuns};
        task->start();
        self->forgetTask(taskId);
    };

    try {
        if (lane == ExecutionLane::Lifecycle) {
            lifecycleExecutor_.post(std::move(runBody));
        } else {
            (void)Promise<void>::async(std::move(runBody));
        }
    } catch (...) {
        releaseTaskRun(taskRuns);
        forgetTask(taskId);
        throw;
    }

    return std::make_shared<HybridSmbTask>(std::move(task));
}

HybridSMB::HybridSMB() : HybridObject(TAG) {
    pool_ = std::make_shared<react_native_smb::SmbConnectionPool>(4, [this](const std::string& taskId) { cancelAllTasksExcept(taskId); });
    observer_ = std::make_unique<react_native_smb::TaskObserverHub>();
}

HybridSMB::~HybridSMB() {
    shutdownImpl();
}

void HybridSMB::shutdownImpl() {
    std::lock_guard<std::mutex> shutdownLock(shutdownMutex_);
    if (disposed_.exchange(true, std::memory_order_acq_rel)) {
        std::unique_lock<std::mutex> lock(taskRuns_->mutex);
        taskRuns_->cv.wait(lock, [&] { return taskRuns_->active == 0; });
        return;
    }

    try {
        cancelAllTasksExcept("__destroy__");
    } catch (...) {
    }
    try {
        if (pool_) pool_->disconnect();
    } catch (...) {
    }
    {
        std::unique_lock<std::mutex> lock(taskRuns_->mutex);
        taskRuns_->cv.wait(lock, [&] { return taskRuns_->active == 0; });
    }
    lifecycleExecutor_.shutdown();
}

// --- lifecycle ---
std::shared_ptr<Promise<void>> HybridSMB::destroy() {
    auto self = shared_cast<HybridSMB>();
    return Promise<void>::async([self]() { self->shutdownImpl(); });
}

void HybridSMB::forgetTask(const std::string& taskId) {
    {
        std::lock_guard<std::mutex> lk(tasksMutex_);
        tasks_.erase(taskId);
    }
    if (observer_) observer_->untrack(taskId);
}

void HybridSMB::cancelTasksMatching(const std::function<bool(const std::shared_ptr<SmbTask>&)>& pred) {
    std::vector<std::shared_ptr<SmbTask>> toCancel;
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        toCancel.reserve(tasks_.size());
        for (const auto& [id, task] : tasks_) {
            if (!task || task->isSettled()) continue;
            if (!pred(task)) continue;
            toCancel.push_back(task);
        }
    }
    if (!toCancel.empty()) SMB_LOG_INFO("Cancelling %zu pending task(s)", toCancel.size());
    for (const auto& task : toCancel) task->cancel();
}

void HybridSMB::cancelAllTasksExcept(const std::string& taskId) {
    cancelTasksMatching([&](const std::shared_ptr<SmbTask>& task) { return task->getId() != taskId; });
}

// --- state ---
bool HybridSMB::isConnected() { return pool_->isConnected(); }

bool HybridSMB::isInitialized() { return pool_->isInitialized(); }

// --- connection ---
std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::initialize(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) {
    return createTask<react_native_smb::InitializeOperator>(taskId, std::make_unique<react_native_smb::InitializeOperator>(url, fromBridge(credentials)), ExecutionLane::Lifecycle);
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::connect(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) {
    return createTask<react_native_smb::ConnectOperator>(taskId, std::make_unique<react_native_smb::ConnectOperator>(url, fromBridge(credentials)), ExecutionLane::Lifecycle);
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::disconnect(const std::string& taskId) {
    return createTask<react_native_smb::DisconnectOperator>(taskId, std::make_unique<react_native_smb::DisconnectOperator>(), ExecutionLane::Lifecycle);
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::listShares(const std::string& taskId) {
    return createTask<react_native_smb::ListSharesOperator>(taskId, std::make_unique<react_native_smb::ListSharesOperator>());
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::connectShare(const std::string& taskId, const std::string& share) {
    return createTask<react_native_smb::ConnectShareOperator>(taskId, std::make_unique<react_native_smb::ConnectShareOperator>(share), ExecutionLane::Lifecycle);
}

// --- listing ---
std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::listDirectory(const std::string& taskId, const std::string& path, double maxDepth, bool includeSecurityDescriptor, double batchSize) {
    return createTask<react_native_smb::ListDirectoryOperator>(taskId, std::make_unique<react_native_smb::ListDirectoryOperator>(path, static_cast<int>(maxDepth), includeSecurityDescriptor, static_cast<int>(batchSize)));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::getPathInfo(const std::string& taskId, const std::string& path) {
    return createTask<react_native_smb::GetPathInfoOperator>(taskId, std::make_unique<react_native_smb::GetPathInfoOperator>(path));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::getSecurityDescriptor(const std::string& taskId, const std::string& path) {
    return createTask<react_native_smb::GetSecurityDescriptorOperator>(taskId, std::make_unique<react_native_smb::GetSecurityDescriptorOperator>(path));
}

// --- transfer ---
std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::downloadFile(const std::string& taskId, const std::string& remotePath, const std::string& localPath) {
    return createTask<react_native_smb::DownloadFileOperator>(taskId, std::make_unique<react_native_smb::DownloadFileOperator>(remotePath, localPath));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::uploadFile(const std::string& taskId, const std::string& localPath, const std::string& remotePath) {
    return createTask<react_native_smb::UploadFileOperator>(taskId, std::make_unique<react_native_smb::UploadFileOperator>(localPath, remotePath));
}

// --- mutate ---
std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::createDirectory(const std::string& taskId, const std::string& path) {
    return createTask<react_native_smb::CreateDirectoryOperator>(taskId, std::make_unique<react_native_smb::CreateDirectoryOperator>(path));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::deleteItem(const std::string& taskId, const std::string& path) {
    return createTask<react_native_smb::DeleteItemOperator>(taskId, std::make_unique<react_native_smb::DeleteItemOperator>(path));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::moveItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath) {
    return createTask<react_native_smb::MoveItemOperator>(taskId, std::make_unique<react_native_smb::MoveItemOperator>(fromPath, toPath));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::renameItem(const std::string& taskId, const std::string& currentPath, const std::string& newName) {
    return createTask<react_native_smb::RenameItemOperator>(taskId, std::make_unique<react_native_smb::RenameItemOperator>(currentPath, newName));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::copyItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath, bool recursive) {
    return createTask<react_native_smb::CopyItemOperator>(taskId, std::make_unique<react_native_smb::CopyItemOperator>(fromPath, toPath, recursive));
}

std::shared_ptr<HybridNativeSmbTaskSpec> HybridSMB::duplicateItem(const std::string& taskId, const std::string& path) {
    return createTask<react_native_smb::DuplicateItemOperator>(taskId, std::make_unique<react_native_smb::DuplicateItemOperator>(path));
}

// --- tasks ---
void HybridSMB::cancelTask(const std::string& taskId) {
    std::shared_ptr<SmbTask> task;
    {
        std::lock_guard<std::mutex> lk(tasksMutex_);
        auto it = tasks_.find(taskId);
        if (it != tasks_.end()) task = it->second;
    }
    if (task) task->cancel();
}

void HybridSMB::cancelTransferTasks() {
    cancelTasksMatching([](const std::shared_ptr<SmbTask>& task) { return isTransferKind(task->getKind()); });
}

// --- pool ---
void HybridSMB::resetPool() {
    if (pool_) pool_->resetPool();
}

namespace {

std::unordered_map<std::string, std::string> encodePoolSlot(const PoolSlotInfo& s) {
    std::unordered_map<std::string, std::string> m;
    m["poolSize"] = std::to_string(s.poolSize);
    m["index"] = std::to_string(s.index);
    m["interactiveOnly"] = s.interactiveOnly ? "1" : "0";
    m["state"] = std::to_string(static_cast<int>(s.state));
    m["kind"] = std::to_string(static_cast<int>(s.kind));
    m["isConnected"] = s.isConnected ? "1" : "0";
    m["shareName"] = s.shareName;
    m["taskId"] = s.taskId;
    return m;
}

}  // namespace

std::vector<std::unordered_map<std::string, std::string>> HybridSMB::getPoolInfo() {
    std::vector<std::unordered_map<std::string, std::string>> out;
    if (!pool_) return out;
    for (const auto& s : pool_->getPoolStatus()) out.push_back(encodePoolSlot(s));
    return out;
}

std::string HybridSMB::subscribePoolInfo(const std::function<void(const std::vector<std::unordered_map<std::string, std::string>>&)>& listener) {
    std::string key = std::to_string(poolListenerToken_.fetch_add(1));
    {
        std::lock_guard<std::mutex> lock(poolListenersMutex_);
        poolListeners_.emplace(key, listener);
    }
    if (pool_) {
        size_t obsId = pool_->addObserver([this, key](const std::vector<PoolSlotInfo>& status) {
            std::vector<std::unordered_map<std::string, std::string>> out;
            for (const auto& s : status) out.push_back(encodePoolSlot(s));
            PoolListener cb;
            {
                std::lock_guard<std::mutex> lock(poolListenersMutex_);
                auto it = poolListeners_.find(key);
                if (it != poolListeners_.end()) cb = it->second;
            }
            if (cb) {
                try {
                    cb(out);
                } catch (...) {
                }
            }
        });
        std::lock_guard<std::mutex> lock(poolListenersMutex_);
        poolObserverIds_[key] = obsId;
    }
    return key;
}

void HybridSMB::unsubscribePoolInfo(const std::string& subscriptionId) {
    size_t obsId = 0;
    {
        std::lock_guard<std::mutex> lock(poolListenersMutex_);
        auto it = poolObserverIds_.find(subscriptionId);
        if (it != poolObserverIds_.end()) {
            obsId = it->second;
            poolObserverIds_.erase(it);
        }
        poolListeners_.erase(subscriptionId);
    }
    if (obsId && pool_) {
        pool_->removeObserver(obsId);
    }
}

// --- tasks ---
std::string HybridSMB::subscribeTaskEvents(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) {
    if (!observer_) return "";
    auto eventId = std::make_shared<std::atomic<uint64_t>>(1);
    return observer_->subscribeAll([listener, eventId](const SmbTaskState& s) {
        std::unordered_map<std::string, std::string> m = TaskSnapshotCodec::encode(s);
        m["eventId"] = std::to_string(eventId->fetch_add(1));
        m["type"] = std::to_string(static_cast<int>(s.status));
        m["ts"] = std::to_string(s.updatedAt);
        listener(m);
    });
}

void HybridSMB::unsubscribeTaskEvents(const std::string& subscriptionId) {
    if (observer_) observer_->unsubscribe(subscriptionId);
}

std::unordered_map<std::string, std::string> HybridSMB::getTask(const std::string& taskId) {
    if (!observer_) return TaskSnapshotCodec::notFound();
    auto snapshot = observer_->getTask(taskId);
    if (!snapshot) return TaskSnapshotCodec::notFound();
    return TaskSnapshotCodec::encode(*snapshot);
}

std::vector<std::unordered_map<std::string, std::string>> HybridSMB::getActiveTasks() {
    std::vector<std::unordered_map<std::string, std::string>> out;
    if (!observer_) return out;
    for (const auto& snapshot : observer_->getActive()) out.push_back(TaskSnapshotCodec::encode(snapshot));
    return out;
}

std::vector<std::unordered_map<std::string, std::string>> HybridSMB::getTransferTasks() {
    std::vector<std::unordered_map<std::string, std::string>> out;
    if (!observer_) return out;
    for (const auto& snapshot : observer_->getTransfers()) out.push_back(TaskSnapshotCodec::encode(snapshot));
    return out;
}

std::vector<std::unordered_map<std::string, std::string>> HybridSMB::getTaskHistory(double limit, double offset) {
    std::vector<std::unordered_map<std::string, std::string>> out;
    if (!observer_) return out;
    const int limitInt = static_cast<int>(limit);
    const int offsetInt = static_cast<int>(offset);
    const size_t safeLimit = limitInt < 0 ? 0 : static_cast<size_t>(limitInt);
    const size_t safeOffset = offsetInt < 0 ? 0 : static_cast<size_t>(offsetInt);
    for (const auto& snapshot : observer_->getHistory(safeLimit, safeOffset)) out.push_back(TaskSnapshotCodec::encode(snapshot));
    return out;
}

void HybridSMB::clearTaskHistory(int64_t beforeTs) {
    if (observer_) observer_->clearHistory(beforeTs);
}

}  // namespace margelo::nitro::smb
