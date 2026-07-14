#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../core/SmbEnums.hpp"
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

    explicit SmbConnectionPool(size_t maxConnections = 4);
    ~SmbConnectionPool();

    // [connection control]
    void initialize(const std::string& url, const SmbCredentials& credentials, const std::string& taskId = "");
    void connect(const std::string& url, const SmbCredentials& credentials, const std::string& taskId = "");
    void connectShare(const std::string& share, const std::string& taskId = "");
    void disconnect(const std::string& taskId = "");
    void resetPool();
    std::vector<SmbShareList> listShares(const std::string& taskId = "");

    // [context]
    PoolContextHandle requestContext(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId = "");
    void releaseContext(const PoolContextHandle& handle);
    void cancelRequestsForTask(const std::string& taskId);

    // [getter]
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
    void tryAssignNext();
    size_t findIdleSlot(AcquireMode mode) const;
    size_t maybeGrowSlot();
    bool slotNeedsActivate(const PoolSlot& slot) const;
    PoolSlot& primarySlot();
    const PoolSlot& primarySlot() const;
    mutable std::mutex mutex_;
    std::condition_variable assignCv_;
    std::vector<PoolSlot> slots_;
    size_t maxConnections_{4};
    ContextRequestQueue queue_;

    std::string serverUrl_;
    std::string shareName_;
    std::shared_ptr<SmbCredentials> credentials_;

    std::map<size_t, PoolObserver> observers_;
    size_t nextObserverId_{1};
};

}  // namespace react_native_smb