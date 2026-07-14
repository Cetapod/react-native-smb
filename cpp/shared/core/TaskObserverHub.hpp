#pragma once

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "SmbTypes.hpp"

namespace react_native_smb {

class SmbTask;

// Optional, per-instance global view of all tasks (transfer tray, debug panel).
// NOT a singleton. The hub SUBSCRIBES to tasks via SmbTask::subscribe(); tasks
// never reference the hub. Removing it leaves the library fully functional.
class TaskObserverHub {
   public:
    TaskObserverHub() = default;
    ~TaskObserverHub();

    // Start/stop watching a task (called by HybridSMB on createTask / teardown).
    void track(const std::shared_ptr<SmbTask>& task);
    void untrack(const std::string& taskId);

    // Downstream subscriptions.
    std::string subscribeAll(SnapshotListener listener);
    std::string subscribeTransfers(SnapshotListener listener);  // download/upload/copy/duplicate
    std::string subscribeActive(SnapshotListener listener);     // Running / Pending
    void unsubscribe(const std::string& id);

    // Queries.
    std::vector<SmbTaskState> getAll() const;
    std::vector<SmbTaskState> getTransfers() const;
    std::vector<SmbTaskState> getActive() const;
    std::vector<SmbTaskState> getHistory(size_t limit = 100, size_t offset = 0) const;
    std::shared_ptr<SmbTaskState> getTask(const std::string& id) const;

    void clearHistory(int64_t beforeTs = 0);

   private:
    void onTaskUpdate(const SmbTaskState& snapshot);  // the listener registered into each task
    void dispatch(const SmbTaskState& snapshot);
    static bool isTransfer(const SmbTaskState& s);
    static bool isActive(const SmbTaskState& s);

    std::unordered_map<std::string, SmbTaskState> tasks_;       // latest snapshot per task
    std::unordered_map<std::string, std::string> taskSubIds_;   // taskId -> sub id in that task
    std::unordered_map<std::string, std::weak_ptr<SmbTask>> tracked_;
    std::deque<SmbTaskState> history_;

    std::map<std::string, SnapshotListener> allSubs_;
    std::map<std::string, SnapshotListener> transferSubs_;
    std::map<std::string, SnapshotListener> activeSubs_;

    mutable std::shared_mutex mutex_;
    std::atomic<size_t> nextSubId_{1};

    static constexpr size_t kHistoryMax = 2000;
};

}  // namespace react_native_smb
