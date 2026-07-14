#include "HybridSMB.hpp"

#include <poll.h>
#include <smb2/libsmb2-dcerpc-srvsvc.h>
#include <smb2/libsmb2-raw.h>

#include <NitroModules/Promise.hpp>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "connection/PoolTypes.hpp"
#include "util/SmbErrorMapper.hpp"
#include "util/SmbLog.hpp"
#include "core/SmbEnums.hpp"
#include "core/TaskSnapshotCodec.hpp"
#include "operators/connection/ConnectOperator.hpp"
#include "operators/connection/ConnectShareOperator.hpp"
#include "operators/connection/DisconnectOperator.hpp"
#include "operators/connection/InitializeOperator.hpp"
#include "operators/connection/ListSharesOperator.hpp"
#include "operators/listing/GetPathInfoOperator.hpp"
#include "operators/listing/GetSecurityDescriptorOperator.hpp"
#include "operators/listing/ListDirectoryOperator.hpp"
#include "operators/mutate/CopyItemOperator.hpp"
#include "operators/mutate/CreateDirectoryOperator.hpp"
#include "operators/mutate/DeleteItemOperator.hpp"
#include "operators/mutate/DuplicateItemOperator.hpp"
#include "operators/mutate/MoveItemOperator.hpp"
#include "operators/mutate/RenameItemOperator.hpp"
#include "operators/transfer/DownloadFileOperator.hpp"
#include "operators/transfer/UploadFileOperator.hpp"

namespace react_native_smb {
using namespace margelo::nitro;

template <typename OpT, typename R>
std::shared_ptr<SmbTask> HybridSMB::createTask(const std::string& taskId, std::unique_ptr<OpT> seedOp) {
    auto task = std::make_shared<SmbTask>(taskId, std::move(seedOp), pool_.get());

    if (observer_) observer_->track(task);
    {
        std::lock_guard<std::mutex> lk(tasksMutex_);
        tasks_[taskId] = task;
    }

    (void)Promise<void>::async([task]() {
      task->start();
    });

    return task;
}

HybridSMB::HybridSMB() : HybridObject(NAME) {
    // 4 parallel SMB connections — each slot is an independent TCP/SMB2 session
    // to the same server/user/share. The pool reserves one slot for Interactive
    // callers (UI ops) so long Metadata fan-outs (parallel copy/delete/list) cannot
    // freeze the UI; see AcquireMode in PoolTypes.hpp.
    pool_ = std::make_unique<SmbConnectionPool>(4);
    observer_ = std::make_unique<TaskObserverHub>();
}

HybridSMB::~HybridSMB() { pool_->disconnect(); }

// --- State Checks ---

bool HybridSMB::isConnected() { return pool_ && pool_->isConnected(); }

bool HybridSMB::isInitialized() { return pool_ && pool_->isInitialized(); }

// --- Connection Management ---

std::shared_ptr<SmbTask> HybridSMB::initialize(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) {
    return createTask<InitializeOperator, void>(taskId, std::make_unique<InitializeOperator>(url, credentials));
}

std::shared_ptr<SmbTask> HybridSMB::connect(const std::string& taskId, const std::string& url, const SmbCredentials& credentials) {
    return createTask<ConnectOperator, SmbConnectionInfo>(taskId, std::make_unique<ConnectOperator>(url, credentials));
}

std::shared_ptr<SmbTask> HybridSMB::disconnect(const std::string& taskId) {
    return createTask<DisconnectOperator, void>(taskId, std::make_unique<DisconnectOperator>());
}

std::shared_ptr<SmbTask> HybridSMB::listShares(const std::string& taskId) {
    return createTask<ListSharesOperator, std::vector<SmbShareList>>(taskId, std::make_unique<ListSharesOperator>());
}

std::shared_ptr<SmbTask> HybridSMB::connectShare(const std::string& taskId, const std::string& share) {
    return createTask<ConnectShareOperator, SmbConnectionInfo>(taskId, std::make_unique<ConnectShareOperator>(share));
}

// --- Listing & Info ---

std::shared_ptr<SmbTask> HybridSMB::listDirectory(const std::string& taskId, const std::string& path, bool recursive, int maxDepth) {
    return createTask<ListDirectoryOperator, std::vector<SmbFileInfo>>(taskId, std::make_unique<ListDirectoryOperator>(path, recursive, maxDepth));
}

std::shared_ptr<SmbTask> HybridSMB::getPathInfo(const std::string& taskId, const std::string& path) {
    return createTask<GetPathInfoOperator, SmbFileInfo>(taskId, std::make_unique<GetPathInfoOperator>(path));
}

std::shared_ptr<SmbTask> HybridSMB::getSecurityDescriptor(const std::string& taskId, const std::string& path) {
    return createTask<GetSecurityDescriptorOperator, SmbSecurityDescriptor>(taskId, std::make_unique<GetSecurityDescriptorOperator>(path));
}

// --- File Transfers ---

std::shared_ptr<SmbTask> HybridSMB::downloadFile(const std::string& taskId, const std::string& remotePath, const std::string& localPath) {
    return createTask<DownloadFileOperator, void>(taskId, std::make_unique<DownloadFileOperator>(remotePath, localPath));
}

std::shared_ptr<SmbTask> HybridSMB::uploadFile(const std::string& taskId, const std::string& localPath, const std::string& remotePath) {
    return createTask<UploadFileOperator, void>(taskId, std::make_unique<UploadFileOperator>(localPath, remotePath));
}

// --- Mutating File Operations ---

std::shared_ptr<SmbTask> HybridSMB::createDirectory(const std::string& taskId, const std::string& path)  {
    return createTask<CreateDirectoryOperator, void>(taskId, std::make_unique<CreateDirectoryOperator>(path));
}

std::shared_ptr<SmbTask> HybridSMB::deleteItem(const std::string& taskId, const std::string& path) {
    return createTask<DeleteItemOperator, void>(taskId, std::make_unique<DeleteItemOperator>(path));
}

std::shared_ptr<SmbTask> HybridSMB::moveItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath) {
    return createTask<MoveItemOperator, void>(taskId, std::make_unique<MoveItemOperator>(fromPath, toPath));
}

std::shared_ptr<SmbTask> HybridSMB::renameItem(const std::string& taskId, const std::string& currentPath, const std::string& newName) {
    return createTask<RenameItemOperator, void>(taskId, std::make_unique<RenameItemOperator>(currentPath, newName));
}

std::shared_ptr<SmbTask> HybridSMB::copyItem(const std::string& taskId, const std::string& fromPath, const std::string& toPath, bool recursive) {
    return createTask<CopyItemOperator, void>(taskId, std::make_unique<CopyItemOperator>(fromPath, toPath, recursive));
}

std::shared_ptr<SmbTask> HybridSMB::duplicateItem(const std::string& taskId, const std::string& path) {
    return createTask<DuplicateItemOperator, std::string>(taskId, std::make_unique<DuplicateItemOperator>(path));
}

void HybridSMB::cancelTask(const std::string& taskId) {
    std::shared_ptr<SmbTask> task;
    {
        std::lock_guard<std::mutex> lk(tasksMutex_);
        auto it = tasks_.find(taskId);
        if (it != tasks_.end()) task = it->second;
    }
    if (task) task->cancel();
}

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

// --- Task observer APIs ---

std::string HybridSMB::subscribeTaskEvents(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) {
    if (!observer_) return "";
    // The hub forwards snapshots
    auto eventId = std::make_shared<std::atomic<uint64_t>>(1);
    return observer_->subscribeAll([listener, eventId](const SmbTaskState& s) {
        std::unordered_map<std::string, std::string> m = TaskSnapshotCodec::encode(s);
        m["eventId"] = std::to_string(eventId->fetch_add(1));
        m["type"] = std::to_string(static_cast<int>(s.status));  // status drives event type
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

std::vector<std::unordered_map<std::string, std::string>> HybridSMB::getTaskHistory(int limit, int offset) {
    std::vector<std::unordered_map<std::string, std::string>> out;
    if (!observer_) return out;
    const size_t safeLimit = limit < 0 ? 0 : static_cast<size_t>(limit);
    const size_t safeOffset = offset < 0 ? 0 : static_cast<size_t>(offset);
    for (const auto& snapshot : observer_->getHistory(safeLimit, safeOffset)) out.push_back(TaskSnapshotCodec::encode(snapshot));
    return out;
}

void HybridSMB::clearTaskHistory(int64_t beforeTs) {
    if (observer_) observer_->clearHistory(beforeTs);
}

void HybridSMB::loadHybridMethods() {
    // Call parent implementation
    HybridObject::loadHybridMethods();

    // Register methods
    registerHybrids(this, [](Prototype& prototype) {
        // --- State Checks ---
        prototype.registerHybridMethod("isConnected", &HybridSMB::isConnected);
        prototype.registerHybridMethod("isInitialized", &HybridSMB::isInitialized);

        // --- Connection Management ---
        prototype.registerHybridMethod("initialize", &HybridSMB::initialize);
        prototype.registerHybridMethod("connect", &HybridSMB::connect);
        prototype.registerHybridMethod("connectShare", &HybridSMB::connectShare);
        prototype.registerHybridMethod("disconnect", &HybridSMB::disconnect);
        prototype.registerHybridMethod("listShares", &HybridSMB::listShares);

        // --- Listing & Info ---
        prototype.registerHybridMethod("listDirectory", &HybridSMB::listDirectory);
        prototype.registerHybridMethod("getPathInfo", &HybridSMB::getPathInfo);
        prototype.registerHybridMethod("getSecurityDescriptor", &HybridSMB::getSecurityDescriptor);

        // --- File Transfers ---
        prototype.registerHybridMethod("downloadFile", &HybridSMB::downloadFile);
        prototype.registerHybridMethod("uploadFile", &HybridSMB::uploadFile);

        // --- Mutating File Operations ---
        prototype.registerHybridMethod("createDirectory", &HybridSMB::createDirectory);
        prototype.registerHybridMethod("deleteItem", &HybridSMB::deleteItem);
        prototype.registerHybridMethod("moveItem", &HybridSMB::moveItem);
        prototype.registerHybridMethod("renameItem", &HybridSMB::renameItem);
        prototype.registerHybridMethod("copyItem", &HybridSMB::copyItem);
        prototype.registerHybridMethod("duplicateItem", &HybridSMB::duplicateItem);

        // Task control
        prototype.registerHybridMethod("subscribeTaskEvents", &HybridSMB::subscribeTaskEvents);
        prototype.registerHybridMethod("unsubscribeTaskEvents", &HybridSMB::unsubscribeTaskEvents);
        prototype.registerHybridMethod("getTask", &HybridSMB::getTask);
        prototype.registerHybridMethod("getActiveTasks", &HybridSMB::getActiveTasks);
        prototype.registerHybridMethod("getTaskHistory", &HybridSMB::getTaskHistory);
        prototype.registerHybridMethod("cancelTask", &HybridSMB::cancelTask);
        prototype.registerHybridMethod("clearTaskHistory", &HybridSMB::clearTaskHistory);

        // Debug / instrumentation
        prototype.registerHybridMethod("subscribePoolInfo", &HybridSMB::subscribePoolInfo);
        prototype.registerHybridMethod("unsubscribePoolInfo", &HybridSMB::unsubscribePoolInfo);
        prototype.registerHybridMethod("getPoolInfo", &HybridSMB::getPoolInfo);
        prototype.registerHybridMethod("resetPool", &HybridSMB::resetPool);
    });
}

}  // namespace react_native_smb
