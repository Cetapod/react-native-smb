#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/SmbEnums.hpp"
#include "../core/CancellationToken.hpp"
#include "ContextRequestQueue.hpp"
#include "PoolContextHandle.hpp"
#include "PoolSlot.hpp"
#include "PoolTypes.hpp"
#include "SmbConnection.hpp"

namespace react_native_smb {

/**
 * Pool layout (maxConnections = N):
 *
 *   slot[0]       — Interactive-only
 *   slot[1..N-1]  — General (any AcquireMode; Interactive may overflow here)
 *
 *   Op::requestContext → Interactive-first FIFO queue → exclusive slot assignment
 */
class SmbConnectionPool {
public:
    using PoolObserver = std::function<void(const std::vector<PoolSlotInfo>&)>;
    using CancelTasksExcept = std::function<void(const std::string&)>;

    explicit SmbConnectionPool(size_t maxConnections = 4, CancelTasksExcept cancelTasksExcept = {});
    ~SmbConnectionPool();

    // [connection control]
    void initialize(const std::string& url, const SmbCredentials& credentials, const std::string& taskId = "", const CancellationToken& cancel = {});
    void connect(const std::string& url, const SmbCredentials& credentials, const std::string& taskId = "", const CancellationToken& cancel = {});
    void connectShare(const std::string& share, const std::string& taskId = "", const CancellationToken& cancel = {});
    void disconnect(const std::string& taskId = "", const CancellationToken& cancel = {});
    void resetPool();
    std::vector<SmbShare> listShares(const std::string& taskId = "");

    // [context]
    PoolContextHandle requestContext(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId = "");
    void releaseContext(const PoolContextHandle& handle);
    void cancelRequestsForTask(const std::string& taskId);

    // [getter]
    size_t maxConnections() const { return maxConnections_; }
    std::string getCurrentUrl() const;
    std::string getServerName() const;
    std::string getShareName() const;
    std::shared_ptr<SmbCredentials> getCredentials() const;
    std::vector<PoolSlotInfo> getPoolStatus() const;

    // [is…]
    bool isConnected() const;
    bool isInitialized() const;

    // [observer]
    size_t addObserver(const PoolObserver& observer);
    void removeObserver(size_t id);

private:
    enum class PoolLifecycle { Running, Closing, Disconnected };

    // Request scheduling
    void tryAssignNext();
    size_t findIdleSlot(AcquireMode mode) const;
    size_t maybeGrowSlot();
    bool slotNeedsActivate(const PoolSlot& slot) const;

    // Lifecycle
    void drainCurrentWork(const std::string& excludeTaskId, const CancellationToken& cancel);
    void disconnectAllSlots(const std::string& taskId);
    void clearConfiguration();

    // Slot access
    PoolSlot& primarySlot();
    const PoolSlot& primarySlot() const;

    // Observers
    void notifyObservers();

    // Synchronization
    mutable std::mutex mutex_;  // Guards pool state below.
    std::mutex lifecycleMutex_; // Serializes lifecycle transitions.
    std::condition_variable assignCv_;

    // Pool state
    std::vector<PoolSlot> slots_;
    size_t maxConnections_{4};
    PoolLifecycle lifecycle_{PoolLifecycle::Disconnected};
    CancelTasksExcept cancelTasksExcept_;

    // Pending context requests
    ContextRequestQueue queue_;
    // Requests outside queue_ while a slot is activating. Cancellation must see them.
    std::unordered_map<uint64_t, ContextRequestPtr> activating_;

    // Connection configuration
    std::string serverUrl_;
    std::string shareName_;
    std::shared_ptr<SmbCredentials> credentials_;

    // Pool observers
    std::map<size_t, PoolObserver> observers_;
    size_t nextObserverId_{1};
};

}  // namespace react_native_smb
