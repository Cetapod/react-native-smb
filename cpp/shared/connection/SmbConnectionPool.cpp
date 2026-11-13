#include "SmbConnectionPool.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include "../util/SmbException.hpp"
#include "../util/SmbLog.hpp"

namespace react_native_smb {

namespace {
void rejectRequests(const std::vector<ContextRequestPtr>& requests, SmbErrorCode code, const std::string& message) {
    for (const auto& req : requests) {
        if (req->callbacks.onRejected) req->callbacks.onRejected(static_cast<int>(code), message);
    }
}
}  // namespace

void SmbConnectionPool::notifyObservers() {
    std::vector<PoolSlotInfo> snap;
    std::map<size_t, PoolObserver> observersCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap.reserve(slots_.size());
        for (const auto& slot : slots_) snap.push_back(slot.snapshot(slots_.size()));
        observersCopy = observers_;
    }
    for (auto& [id, fn] : observersCopy) {
        try {
            if (fn) fn(snap);
        } catch (...) {
        }
    }
}

SmbConnectionPool::SmbConnectionPool(size_t maxConnections, CancelTasksExcept cancelTasksExcept)
    : maxConnections_(maxConnections), cancelTasksExcept_(std::move(cancelTasksExcept)) {
    slots_.reserve(maxConnections);
    slots_.emplace_back(0);
}

SmbConnectionPool::~SmbConnectionPool() {
    for (auto& slot : slots_) {
        if (slot.manager()) slot.manager()->disconnect();
    }
}

PoolSlot& SmbConnectionPool::primarySlot() { return slots_[0]; }

const PoolSlot& SmbConnectionPool::primarySlot() const { return slots_[0]; }

bool SmbConnectionPool::slotNeedsActivate(const PoolSlot& slot) const {
    const auto* mgr = slot.manager();
    if (!mgr) return true;
    return !mgr->isInitialized() && !serverUrl_.empty() && credentials_ != nullptr;
}

size_t SmbConnectionPool::findIdleSlot(AcquireMode mode) const {
    if (mode == AcquireMode::Interactive) {
        if (slots_[0].isIdle()) return 0;
        for (size_t i = 1; i < slots_.size(); ++i) {
            if (slots_[i].isIdle()) return i;
        }
        return SIZE_MAX;
    }
    for (size_t i = 1; i < slots_.size(); ++i) {
        if (slots_[i].isIdle()) return i;
    }
    return SIZE_MAX;
}

size_t SmbConnectionPool::maybeGrowSlot() {
    if (slots_.size() >= maxConnections_) return SIZE_MAX;
    const size_t idx = slots_.size();
    slots_.emplace_back(idx);
    return idx;
}

void SmbConnectionPool::tryAssignNext() {
    std::vector<std::pair<ContextRequestPtr, size_t>> granted;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (lifecycle_ == PoolLifecycle::Running) {
            ContextRequestPtr req = queue_.takeNext();
            if (!req) break;
            size_t slotIdx = findIdleSlot(req->mode);
            if (slotIdx == SIZE_MAX) slotIdx = maybeGrowSlot();
            if (slotIdx == SIZE_MAX) {
                queue_.requeueFront(std::move(req));
                break;
            }
            slots_[slotIdx].markActivating();
            activating_[req->id] = req;
            granted.emplace_back(std::move(req), slotIdx);
        }
    }
    if (granted.empty()) return;
    notifyObservers();
    for (auto& [req, slotIdx] : granted) req->callbacks.onGranted(SlotReservation(this, slotIdx, req));
}

void SmbConnectionPool::requestContext(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId, ContextCallbacks callbacks) {
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        accepted = lifecycle_ == PoolLifecycle::Running;
        if (accepted) queue_.enqueue(mode, kind, taskId, std::move(callbacks));
    }
    if (!accepted) {
        if (callbacks.onRejected) callbacks.onRejected(static_cast<int>(SmbErrorCode::NotConnected), "SMB connection is closing or disconnected");
        return;
    }
    tryAssignNext();
}

PoolContextHandle SmbConnectionPool::activateReservation(size_t slotIndex, const ContextRequestPtr& req) {
    PoolSlot& slot = slots_[slotIndex];
    PoolConnectParams params;
    bool needsActivate = false;
    bool live = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        live = lifecycle_ == PoolLifecycle::Running && !req->cancelled.load(std::memory_order_acquire);
        params.serverUrl = serverUrl_;
        params.shareName = shareName_;
        params.credentials = credentials_;
        params.capabilities = capabilities_;
        needsActivate = slotNeedsActivate(slot);
    }

    // The slot is Activating, so no other thread touches it while it connects unlocked.
    std::optional<SmbException> failure;
    if (live) {
        try {
            if (needsActivate)
                slot.activate(params, req->taskId);
            else
                slot.ensureShare(params.shareName, req->taskId);
        } catch (const SmbException& e) {
            failure.emplace(e);
        } catch (const std::exception& e) {
            failure.emplace(static_cast<SmbErrorCode>(SmbErrorMapper::fromErrnoOrMessage(0, e.what())), e.what());
        } catch (...) {
            failure.emplace(SmbErrorCode::NotConnected, "context request failed");
        }
    }

    std::optional<PoolContextHandle> handle;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activating_.erase(req->id);
        if (lifecycle_ != PoolLifecycle::Running || req->cancelled.load(std::memory_order_acquire)) {
            if (failure) {
                slot.resetBroken();
            } else {
                slot.release();
            }
            failure.emplace(SmbErrorCode::Cancelled, "context request cancelled");
        } else if (failure) {
            slot.resetBroken();
        } else {
            slot.assign(req->id, req->taskId, req->kind);
            slot.manager()->setCapabilities(params.capabilities);
            handle.emplace(this, slotIndex, slot.manager());
        }
        assignCv_.notify_all();
    }
    notifyObservers();

    if (handle) return std::move(*handle);
    if (failure->code() != SmbErrorCode::Cancelled) {
        SMB_LOG_ERROR("Pool could not activate a context for task %s (%s)", req->taskId.c_str(), operationName(req->kind));
    }
    tryAssignNext();
    throw *failure;
}

void SmbConnectionPool::abandonReservation(size_t slotIndex, const ContextRequestPtr& req) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activating_.erase(req->id);
        slots_[slotIndex].release();
        assignCv_.notify_all();
    }
    notifyObservers();
    tryAssignNext();
}

void SmbConnectionPool::releaseContext(const PoolContextHandle& handle) {
    bool released = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle.valid() && handle.slotIndex() < slots_.size()) {
            slots_[handle.slotIndex()].release();
            released = true;
        }
        assignCv_.notify_all();
    }
    if (released) notifyObservers();
    tryAssignNext();
}

void SmbConnectionPool::cancelRequestsForTask(const std::string& taskId) {
    std::vector<ContextRequestPtr> rejected;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rejected = queue_.takeForTask(taskId);
        for (auto& [id, req] : activating_) {
            if (req->taskId == taskId) req->cancelled.store(true, std::memory_order_release);
        }
    }
    rejectRequests(rejected, SmbErrorCode::Cancelled, "context request cancelled");
}

size_t SmbConnectionPool::addObserver(const PoolObserver& observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t id = nextObserverId_++;
    observers_.emplace(id, observer);
    return id;
}

void SmbConnectionPool::removeObserver(size_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(id);
}

std::vector<PoolSlotInfo> SmbConnectionPool::getPoolStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PoolSlotInfo> out;
    out.reserve(slots_.size());
    for (const auto& slot : slots_) out.push_back(slot.snapshot(slots_.size()));
    return out;
}

void SmbConnectionPool::drainCurrentWork(const std::string& excludeTaskId, const CancellationToken& cancel) {
    if (cancel.cancelled()) SmbException::raise(SmbErrorCode::Cancelled, "connection transition cancelled");
    std::vector<ContextRequestPtr> rejected;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lifecycle_ = PoolLifecycle::Closing;
        rejected = queue_.takeAll();
    }
    rejectRequests(rejected, SmbErrorCode::Cancelled, "context request cancelled");
    if (cancelTasksExcept_) cancelTasksExcept_(excludeTaskId);
    std::unique_lock<std::mutex> lock(mutex_);
    assignCv_.wait(lock, [&] {
        for (const auto& slot : slots_) {
            if (!slot.isIdle()) return false;
        }
        return true;
    });
}

void SmbConnectionPool::disconnectAllSlots(const std::string& taskId) {
    for (auto& slot : slots_) {
        if (slot.manager()) slot.manager()->disconnect(taskId);
    }
}

void SmbConnectionPool::clearConfiguration() {
    serverUrl_.clear();
    shareName_.clear();
    credentials_.reset();
    capabilities_ = std::make_shared<ServerCapabilities>();
}

void SmbConnectionPool::initialize(const std::string& url, const SmbCredentials& credentials, const std::string& taskId, const CancellationToken& cancel) {
    std::lock_guard<std::mutex> transitionLock(lifecycleMutex_);
    drainCurrentWork(taskId, cancel);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        disconnectAllSlots(taskId);
        clearConfiguration();
        serverUrl_ = url;
        credentials_ = std::make_shared<SmbCredentials>(credentials);
        primarySlot().manager()->initialize(url, credentials, taskId);
        lifecycle_ = PoolLifecycle::Running;
        SMB_LOG_INFO("Pool initialized");
    } catch (...) {
        disconnectAllSlots(taskId);
        clearConfiguration();
        lifecycle_ = PoolLifecycle::Disconnected;
        SMB_LOG_ERROR("Pool initialization failed");
        throw;
    }
}

void SmbConnectionPool::connect(const std::string& url, const SmbCredentials& credentials, const std::string& taskId, const CancellationToken& cancel) {
    std::lock_guard<std::mutex> transitionLock(lifecycleMutex_);
    drainCurrentWork(taskId, cancel);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        disconnectAllSlots(taskId);
        clearConfiguration();
        serverUrl_ = url;
        credentials_ = std::make_shared<SmbCredentials>(credentials);
        if (url.size() > 6 && url.substr(0, 6) == "smb://") {
            std::string remaining = url.substr(6);
            const size_t slash = remaining.find('/');
            if (slash != std::string::npos) shareName_ = remaining.substr(slash + 1);
        }
        primarySlot().manager()->connect(url, credentials, taskId);
        lifecycle_ = PoolLifecycle::Running;
        SMB_LOG_INFO("Pool connected");
    } catch (...) {
        disconnectAllSlots(taskId);
        clearConfiguration();
        lifecycle_ = PoolLifecycle::Disconnected;
        SMB_LOG_ERROR("Pool connection failed");
        throw;
    }
}

void SmbConnectionPool::connectShare(const std::string& share, const std::string& taskId, const CancellationToken& cancel) {
    std::lock_guard<std::mutex> transitionLock(lifecycleMutex_);
    drainCurrentWork(taskId, cancel);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        primarySlot().manager()->connectShare(share, taskId);
        for (size_t i = 1; i < slots_.size(); ++i) {
            slots_[i].manager()->disconnect(taskId);
        }
        shareName_ = share;
        capabilities_ = std::make_shared<ServerCapabilities>();
        lifecycle_ = PoolLifecycle::Running;
        SMB_LOG_INFO("Pool share connected");
    } catch (...) {
        for (size_t i = 1; i < slots_.size(); ++i) {
            slots_[i].manager()->disconnect(taskId);
        }
        shareName_.clear();
        lifecycle_ = PoolLifecycle::Running;
        SMB_LOG_ERROR("Pool share connection failed");
        throw;
    }
}

void SmbConnectionPool::disconnect(const std::string& taskId, const CancellationToken& cancel) {
    std::lock_guard<std::mutex> transitionLock(lifecycleMutex_);
    drainCurrentWork(taskId, cancel);
    size_t slotCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disconnectAllSlots(taskId);
        clearConfiguration();
        lifecycle_ = PoolLifecycle::Disconnected;
        slotCount = slots_.size();
    }
    SMB_LOG_INFO("Pool disconnected (%zu slot(s))", slotCount);
    notifyObservers();
}

void SmbConnectionPool::resetPool() {
    std::lock_guard<std::mutex> transitionLock(lifecycleMutex_);
    drainCurrentWork("", {});
    size_t slotCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disconnectAllSlots("");
        if (slots_.size() > 1) slots_.erase(slots_.begin() + 1, slots_.end());
        slots_[0].resetBroken();
        clearConfiguration();
        lifecycle_ = PoolLifecycle::Disconnected;
        slotCount = slots_.size();
    }
    SMB_LOG_INFO("Pool reset (%zu slot(s))", slotCount);
    notifyObservers();
}

bool SmbConnectionPool::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return primarySlot().manager() && primarySlot().manager()->isConnected();
}

bool SmbConnectionPool::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return primarySlot().manager() && primarySlot().manager()->isInitialized();
}

std::string SmbConnectionPool::getCurrentUrl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return primarySlot().manager() ? primarySlot().manager()->getCurrentUrl() : std::string();
}

std::string SmbConnectionPool::getServerName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return primarySlot().manager() ? primarySlot().manager()->getServerName() : std::string();
}

std::string SmbConnectionPool::getShareName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shareName_;
}

std::shared_ptr<SmbCredentials> SmbConnectionPool::getCredentials() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return credentials_;
}

}  // namespace react_native_smb
