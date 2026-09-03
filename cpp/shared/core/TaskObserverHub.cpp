#include "TaskObserverHub.hpp"

#include "SmbTask.hpp"

namespace react_native_smb {

TaskObserverHub::~TaskObserverHub() {
    // Unsubscribe from any still-alive tasks.
    std::unique_lock lock(mutex_);
    for (auto& [taskId, weak] : tracked_) {
        if (auto task = weak.lock()) {
            auto it = taskSubIds_.find(taskId);
            if (it != taskSubIds_.end()) task->unsubscribe(it->second);
        }
    }
}

void TaskObserverHub::track(const std::shared_ptr<SmbTask>& task) {
    if (!task) return;
    // Register as a normal subscriber; the task does not know this is "the hub".
    std::string subId = task->subscribe([this](const SmbTaskState& s) { onTaskUpdate(s); });
    std::unique_lock lock(mutex_);
    tracked_[task->getId()] = task;
    taskSubIds_[task->getId()] = subId;
}

void TaskObserverHub::untrack(const std::string& taskId) {
    std::shared_ptr<SmbTask> task;
    std::string subId;
    {
        std::unique_lock lock(mutex_);
        if (auto it = tracked_.find(taskId); it != tracked_.end()) {
            task = it->second.lock();
            tracked_.erase(it);
        }
        if (auto it = taskSubIds_.find(taskId); it != taskSubIds_.end()) {
            subId = it->second;
            taskSubIds_.erase(it);
        }
    }
    if (task && !subId.empty()) task->unsubscribe(subId);
}

void TaskObserverHub::onTaskUpdate(const SmbTaskState& snapshot) {
    {
        std::unique_lock lock(mutex_);
        tasks_[snapshot.taskId] = snapshot;
        if (snapshot.endedAt != 0) {
            history_.push_back(snapshot);
            while (history_.size() > kHistoryMax) history_.pop_front();
        }
    }
    dispatch(snapshot);
}

void TaskObserverHub::dispatch(const SmbTaskState& snapshot) {
    std::vector<SnapshotListener> all, transfer, active;
    {
        std::shared_lock lock(mutex_);
        for (auto& [id, fn] : allSubs_) all.push_back(fn);
        if (isTransfer(snapshot))
            for (auto& [id, fn] : transferSubs_) transfer.push_back(fn);
        if (isActive(snapshot))
            for (auto& [id, fn] : activeSubs_) active.push_back(fn);
    }
    for (auto& fn : all)
        if (fn) fn(snapshot);
    for (auto& fn : transfer)
        if (fn) fn(snapshot);
    for (auto& fn : active)
        if (fn) fn(snapshot);
}

bool TaskObserverHub::isTransfer(const SmbTaskState& s) { return isTransferKind(s.kind); }

bool TaskObserverHub::isActive(const SmbTaskState& s) { return s.status == SmbTaskStatus::Running || s.status == SmbTaskStatus::Pending; }

std::string TaskObserverHub::subscribeAll(SnapshotListener listener) {
    std::unique_lock lock(mutex_);
    std::string id = "all_" + std::to_string(nextSubId_++);
    allSubs_[id] = std::move(listener);
    return id;
}

std::string TaskObserverHub::subscribeTransfers(SnapshotListener listener) {
    std::unique_lock lock(mutex_);
    std::string id = "txf_" + std::to_string(nextSubId_++);
    transferSubs_[id] = std::move(listener);
    return id;
}

std::string TaskObserverHub::subscribeActive(SnapshotListener listener) {
    std::unique_lock lock(mutex_);
    std::string id = "act_" + std::to_string(nextSubId_++);
    activeSubs_[id] = std::move(listener);
    return id;
}

void TaskObserverHub::unsubscribe(const std::string& id) {
    std::unique_lock lock(mutex_);
    allSubs_.erase(id);
    transferSubs_.erase(id);
    activeSubs_.erase(id);
}

std::vector<SmbTaskState> TaskObserverHub::getAll() const {
    std::shared_lock lock(mutex_);
    std::vector<SmbTaskState> out;
    out.reserve(tasks_.size());
    for (auto& [id, s] : tasks_) out.push_back(s);
    return out;
}

std::vector<SmbTaskState> TaskObserverHub::getTransfers() const {
    std::shared_lock lock(mutex_);
    std::vector<SmbTaskState> out;
    for (auto& [id, s] : tasks_)
        if (isTransfer(s)) out.push_back(s);
    return out;
}

std::vector<SmbTaskState> TaskObserverHub::getActive() const {
    std::shared_lock lock(mutex_);
    std::vector<SmbTaskState> out;
    for (auto& [id, s] : tasks_)
        if (isActive(s)) out.push_back(s);
    return out;
}

std::vector<SmbTaskState> TaskObserverHub::getHistory(size_t limit, size_t offset) const {
    std::shared_lock lock(mutex_);
    std::vector<SmbTaskState> out;
    if (offset >= history_.size()) return out;
    // history_ is oldest-first; return newest-first for UI.
    const size_t total = history_.size();
    size_t startFromEnd = offset;
    size_t count = 0;
    for (size_t i = 0; i < total && count < limit; ++i) {
        size_t idx = total - 1 - i;  // newest first
        if (i < startFromEnd) continue;
        out.push_back(history_[idx]);
        ++count;
    }
    return out;
}

std::shared_ptr<SmbTaskState> TaskObserverHub::getTask(const std::string& id) const {
    std::shared_lock lock(mutex_);
    if (auto it = tasks_.find(id); it != tasks_.end()) return std::make_shared<SmbTaskState>(it->second);
    return nullptr;
}

void TaskObserverHub::clearHistory(int64_t beforeTs) {
    std::unique_lock lock(mutex_);
    if (beforeTs <= 0) {
        history_.clear();
        // Drop terminal snapshots from the latest-snapshot map; keep active ones.
        for (auto it = tasks_.begin(); it != tasks_.end();) {
            if (it->second.endedAt != 0)
                it = tasks_.erase(it);
            else
                ++it;
        }
        return;
    }
    std::deque<SmbTaskState> kept;
    for (auto& s : history_) {
        if (s.endedAt >= beforeTs) kept.push_back(s);
    }
    history_.swap(kept);
    for (auto it = tasks_.begin(); it != tasks_.end();) {
        if (it->second.endedAt != 0 && it->second.endedAt < beforeTs)
            it = tasks_.erase(it);
        else
            ++it;
    }
}

}  // namespace react_native_smb
