#include "SmbConnectionPool.hpp"

#include <stdexcept>
#include <utility>

#include "../util/SmbException.hpp"
#include "../util/SmbLog.hpp"

namespace react_native_smb {

namespace {
const char* acquireModeName(AcquireMode mode) {
    return mode == AcquireMode::Interactive ? "interactive" : "metadata";
}

bool fulfillRequest(const ContextRequestPtr& req, PoolContextHandle handle) {
    if (!req || req->fulfilled.exchange(true, std::memory_order_acq_rel)) return false;
    try {
        req->promise.set_value(std::move(handle));
        return true;
    } catch (...) {
        return false;
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
    bool assigned = false;
    for (;;) {
        ContextRequestPtr req;
        size_t slotIdx = SIZE_MAX;
        bool needsActivate = false;
        PoolConnectParams params;
        std::string taskId;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (lifecycle_ != PoolLifecycle::Running) {
                SMB_LOG("Pool assign skip");
                break;
            }
            queue_.removeCancelledFromHead();
            req = queue_.takeNext();
            if (!req) break;

            slotIdx = findIdleSlot(req->mode);
            if (slotIdx == SIZE_MAX && canGrowPoolSlot(req->kind)) slotIdx = maybeGrowSlot();
            if (slotIdx == SIZE_MAX) {
                queue_.requeueFront(std::move(req));
                break;
            }

            PoolSlot& slot = slots_[slotIdx];
            if (!slot.accepts(req->mode)) {
                queue_.requeueFront(std::move(req));
                break;
            }

            slot.markActivating();
            activating_[req->id] = req;
            params.serverUrl = serverUrl_;
            params.shareName = shareName_;
            params.credentials = credentials_;
            needsActivate = slotNeedsActivate(slot);
            taskId = req->taskId;

            lock.unlock();

            bool ok = true;
            std::string activationError;
            int activationCode = static_cast<int>(SmbErrorCode::NotConnected);
            try {
                if (needsActivate)
                    slot.activate(params, taskId);
                else
                    slot.ensureShare(shareName_, taskId);
            } catch (const SmbException& e) {
                ok = false;
                activationError = e.what();
                activationCode = e.codeInt();
            } catch (const std::exception& e) {
                ok = false;
                activationError = e.what();
                activationCode = SmbErrorMapper::fromErrnoOrMessage(0, e.what());
            } catch (...) {
                ok = false;
                activationError = "context request failed";
                activationCode = static_cast<int>(SmbErrorCode::NotConnected);
            }

            lock.lock();
            activating_.erase(req->id);

            const bool wasCancelled = req->cancelled.load(std::memory_order_acquire);

            if (lifecycle_ != PoolLifecycle::Running || wasCancelled) {
                SMB_LOG("Pool assign abort slot=%zu kind=%s taskId=%s cancelled=%d", slotIdx, operationName(req->kind),
                        req->taskId.c_str(), wasCancelled ? 1 : 0);
                slot.resetBroken();
                req->cancelled.store(true, std::memory_order_release);
                fulfillRequest(req, PoolContextHandle{});
                assignCv_.notify_all();
                if (lifecycle_ != PoolLifecycle::Running) break;
                continue;
            }

            if (!ok) {
                SMB_LOG("Pool assign fail slot=%zu kind=%s taskId=%s", slotIdx, operationName(req->kind), req->taskId.c_str());
                slot.resetBroken();
                req->errorMessage = activationError.empty() ? "context request failed" : std::move(activationError);
                req->errorCode = activationCode;
                fulfillRequest(req, PoolContextHandle{});
                assignCv_.notify_all();
                continue;
            }

            slot.assign(req->id, req->taskId, req->kind);
            SMB_LOG("Pool assign slot=%zu kind=%s taskId=%s mode=%s", slotIdx, operationName(req->kind), req->taskId.c_str(),
                    acquireModeName(req->mode));
            PoolContextHandle handle(this, slotIdx, slot.manager());
            if (!fulfillRequest(req, std::move(handle))) {
                // The unconsumed handle releases the slot when it leaves scope.
                assignCv_.notify_all();
                continue;
            }
            assigned = true;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        assignCv_.notify_all();
    }
    if (assigned) notifyObservers();
}

PoolContextHandle SmbConnectionPool::requestContext(AcquireMode mode, SmbOperatorKind kind, const std::string& taskId) {
    ContextRequestPtr req;
    std::future<PoolContextHandle> fut;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lifecycle_ != PoolLifecycle::Running) {
            SmbException::raise(SmbErrorCode::NotConnected, "SMB connection is closing or disconnected");
        }
        req = queue_.enqueue(mode, kind, taskId);
        fut = req->promise.get_future();
    }
    tryAssignNext();

    std::unique_lock<std::mutex> lock(mutex_);
    assignCv_.wait(lock, [&] {
        return req->fulfilled.load(std::memory_order_acquire) || req->cancelled.load(std::memory_order_acquire);
    });

    if (req->cancelled.load(std::memory_order_acquire)) {
        SmbException::raise(SmbErrorCode::Cancelled, "context request cancelled");
    }

    PoolContextHandle handle = fut.get();
    if (!handle.valid()) {
        const std::string msg = req->errorMessage.empty() ? "context request failed" : req->errorMessage;
        SmbException::raise(req->errorCode == 0 ? static_cast<int>(SmbErrorCode::NotConnected) : req->errorCode, msg);
    }
    return handle;
}

void SmbConnectionPool::releaseContext(const PoolContextHandle& handle) {
    bool shouldAssign = false;
    bool released = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle.valid() && handle.slotIndex() < slots_.size()) {
            const size_t idx = handle.slotIndex();
            SMB_LOG("Pool release slot=%zu", idx);
            slots_[idx].release();
            released = true;
        }
        shouldAssign = lifecycle_ == PoolLifecycle::Running;
        assignCv_.notify_all();
    }
    if (released) notifyObservers();
    if (shouldAssign) {
        tryAssignNext();
    }
}

void SmbConnectionPool::cancelRequestsForTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.cancelForTask(taskId);
    for (auto& [id, req] : activating_) {
        if (!req || req->taskId != taskId) continue;
        if (req->fulfilled.load(std::memory_order_acquire)) continue;
        req->cancelled.store(true, std::memory_order_release);
        // Do not fulfill here — tryAssignNext owns the promise once activation returns.
    }
    assignCv_.notify_all();
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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lifecycle_ = PoolLifecycle::Closing;
        queue_.cancelAll();
        assignCv_.notify_all();
    }
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
}

void SmbConnectionPool::initialize(const std::string& url, const SmbCredentials& credentials, const std::string& taskId, const CancellationToken& cancel) {
    SMB_LOG("Pool transition initialize start taskId=%s", taskId.c_str());
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
        SMB_LOG("Pool transition initialize done taskId=%s lifecycle=Running", taskId.c_str());
    } catch (...) {
        disconnectAllSlots(taskId);
        clearConfiguration();
        lifecycle_ = PoolLifecycle::Disconnected;
        SMB_LOG("Pool transition initialize failed taskId=%s lifecycle=Disconnected", taskId.c_str());
        throw;
    }
}

void SmbConnectionPool::connect(const std::string& url, const SmbCredentials& credentials, const std::string& taskId, const CancellationToken& cancel) {
    SMB_LOG("Pool transition connect start taskId=%s url=%s", taskId.c_str(), url.c_str());
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
        SMB_LOG("Pool transition connect done taskId=%s lifecycle=Running share=%s", taskId.c_str(), shareName_.c_str());
    } catch (...) {
        disconnectAllSlots(taskId);
        clearConfiguration();
        lifecycle_ = PoolLifecycle::Disconnected;
        SMB_LOG("Pool transition connect failed taskId=%s lifecycle=Disconnected", taskId.c_str());
        throw;
    }
}

void SmbConnectionPool::connectShare(const std::string& share, const std::string& taskId, const CancellationToken& cancel) {
    SMB_LOG("Pool transition connectShare start taskId=%s share=%s", taskId.c_str(), share.c_str());
    std::lock_guard<std::mutex> transitionLock(lifecycleMutex_);
    drainCurrentWork(taskId, cancel);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        primarySlot().manager()->connectShare(share, taskId);
        for (size_t i = 1; i < slots_.size(); ++i) {
            slots_[i].manager()->disconnect(taskId);
        }
        shareName_ = share;
        lifecycle_ = PoolLifecycle::Running;
        SMB_LOG("Pool transition connectShare done taskId=%s lifecycle=Running share=%s secondarySlots=%zu", taskId.c_str(),
                share.c_str(), slots_.size() > 1 ? slots_.size() - 1 : 0);
    } catch (...) {
        for (size_t i = 1; i < slots_.size(); ++i) {
            slots_[i].manager()->disconnect(taskId);
        }
        shareName_.clear();
        lifecycle_ = PoolLifecycle::Running;
        SMB_LOG("Pool transition connectShare failed taskId=%s lifecycle=Running shareCleared=1", taskId.c_str());
        throw;
    }
}

void SmbConnectionPool::disconnect(const std::string& taskId, const CancellationToken& cancel) {
    SMB_LOG("Pool transition disconnect start taskId=%s", taskId.c_str());
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
    SMB_LOG("Pool transition disconnect done taskId=%s lifecycle=Disconnected slots=%zu", taskId.c_str(), slotCount);
    notifyObservers();
}

void SmbConnectionPool::resetPool() {
    SMB_LOG("Pool transition resetPool start");
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
    SMB_LOG("Pool transition resetPool done lifecycle=Disconnected slots=%zu", slotCount);
    notifyObservers();
}

std::vector<SmbShareList> SmbConnectionPool::listShares(const std::string& taskId) {
    auto handle = requestContext(AcquireMode::Interactive, SmbOperatorKind::ListShares, taskId);
    return handle.manager().listShares(taskId);
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
