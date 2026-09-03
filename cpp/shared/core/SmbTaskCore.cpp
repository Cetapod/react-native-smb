#include "SmbTaskCore.hpp"

#include <algorithm>
#include <chrono>

#include "../operators/OperatorBase.hpp"
#include "../util/SmbErrorMapper.hpp"
#include "../util/SmbLog.hpp"

namespace react_native_smb {

namespace jsi = facebook::jsi;

int64_t SmbTaskCore::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

SmbTaskCore::SmbTaskCore(std::string id, SmbOperatorKind kind)
    : id_(std::move(id)), kind_(kind) {}

SmbTaskCore::~SmbTaskCore() = default;

void SmbTaskCore::cancel() {
    cancel_.cancel();
}

SmbTaskState SmbTaskCore::getState() const {
    SmbTaskState s;
    writeToSnapshot(s);
    return s;
}

void SmbTaskCore::setPaths(std::string source, std::string destination) {
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        if (settled_.load(std::memory_order_acquire)) return;
        sourcePath_ = std::move(source);
        destinationPath_ = std::move(destination);
        updatedAt_ = nowMs();
    }
    notifyListeners();
}

void SmbTaskCore::addExpectedBytes(int64_t bytes) {
    if (settled_.load(std::memory_order_acquire)) return;
    totalBytes_.fetch_add(bytes);
    notifyListeners();
}

void SmbTaskCore::onOpStatus(size_t opIndex, SmbTaskStatus s, const std::string& error, int code) {
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        if (settled_.load(std::memory_order_acquire)) return;
        if (status_.load() == SmbTaskStatus::Cancelled && s == SmbTaskStatus::Success) return;
        if (opIndex < opProgress_.size()) opProgress_[opIndex].status = s;
        if (s == SmbTaskStatus::Error && status_.load() != SmbTaskStatus::Error) {
            errorMessage_ = error;
            errorCode_ = code;
        }
    }
    notifyListeners();
}

void SmbTaskCore::onOpProgress(size_t opIndex, double done, double total) {
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        if (settled_.load(std::memory_order_acquire)) return;
        if (opIndex < opProgress_.size()) {
            opProgress_[opIndex].done = static_cast<int64_t>(done);
            opProgress_[opIndex].total = static_cast<int64_t>(total);
        }
        recomputeAggregateLocked();
    }
    notifyListeners();
}

std::string SmbTaskCore::subscribe(SnapshotListener listener) {
    std::lock_guard<std::mutex> lk(listenersMutex_);
    std::string id = "lst_" + std::to_string(nextListenerId_++);
    listeners_[id] = std::move(listener);
    return id;
}

void SmbTaskCore::unsubscribe(const std::string& id) {
    std::lock_guard<std::mutex> lk(listenersMutex_);
    listeners_.erase(id);
}

void SmbTaskCore::publishResult(std::any value,
                                std::function<jsi::Value(jsi::Runtime&, const std::any&)> converter) {
    if (settled_.load(std::memory_order_acquire)) return;
    resultValue_ = std::move(value);
    resultConverter_ = std::move(converter);
}

jsi::Value SmbTaskCore::getResultValue(jsi::Runtime& runtime) const {
    if (!resultConverter_) return jsi::Value::undefined();
    try {
        return resultConverter_(runtime, resultValue_);
    } catch (...) {
        return jsi::Value::undefined();
    }
}

std::future<void> SmbTaskCore::completionFuture() { return done_.get_future(); }

void SmbTaskCore::markPending() {
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        if (settled_.load(std::memory_order_acquire)) return;
        status_ = SmbTaskStatus::Pending;
        if (startedAt_ == 0) startedAt_ = nowMs();
        updatedAt_ = nowMs();
    }
    notifyListeners();
}

void SmbTaskCore::markRunning() {
    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        if (settled_.load(std::memory_order_acquire)) return;
        status_ = SmbTaskStatus::Running;
        if (startedAt_ == 0) startedAt_ = nowMs();
        updatedAt_ = nowMs();
    }
    notifyListeners();
}

void SmbTaskCore::onOpFinished() {
    const size_t before = pending_.fetch_sub(1);
    if (before == 1) maybeSettle();
}

void SmbTaskCore::recomputeAggregateLocked() {
    updatedAt_ = nowMs();
    if (!isDeterminate(kind_)) {
        progress_ = 0.0;
        return;
    }
    int64_t d = 0, t = 0;
    for (const auto& p : opProgress_) {
        d += p.done;
        t += p.total;
    }
    bytesDone_ = d;
    const int64_t denom = std::max<int64_t>(t, std::max<int64_t>(d, totalBytes_.load()));
    totalBytes_ = denom;
    progress_ = (denom > 0) ? (static_cast<double>(d) / static_cast<double>(denom)) : 0.0;

    const int64_t elapsedMs = updatedAt_ - startedAt_;
    if (elapsedMs > 0 && d > 0) {
        const double elapsedSec = elapsedMs / 1000.0;
        bytesPerSecond_ = static_cast<double>(d) / elapsedSec;
        etaSeconds_ = (denom > d && bytesPerSecond_ > 0.0) ? (static_cast<double>(denom - d) / bytesPerSecond_) : 0.0;
    }
}

void SmbTaskCore::writeToSnapshot(SmbTaskState& out) const {
    std::lock_guard<std::mutex> lk(stateMutex_);
    out.taskId = id_;
    out.kind = kind_;
    out.status = status_.load();
    out.progress = progress_.load();
    out.errorCode = errorCode_.load();
    out.errorMessage = errorMessage_;
    out.sourcePath = sourcePath_;
    out.destinationPath = destinationPath_;
    out.bytesDone = bytesDone_.load();
    out.totalBytes = totalBytes_.load();
    out.bytesPerSecond = bytesPerSecond_;
    out.etaSeconds = etaSeconds_;
    out.startedAt = startedAt_;
    out.updatedAt = updatedAt_;
    out.endedAt = endedAt_;
}

void SmbTaskCore::notifyListeners() {
    SmbTaskState snap;
    writeToSnapshot(snap);
    std::vector<SnapshotListener> listenersCopy;
    {
        std::lock_guard<std::mutex> lk(listenersMutex_);
        listenersCopy.reserve(listeners_.size());
        for (auto& [id, fn] : listeners_) {
            if (fn) listenersCopy.push_back(fn);
        }
    }
    for (auto& fn : listenersCopy) {
        try {
            if (fn) fn(snap);
        } catch (...) {
        }
    }
}

void SmbTaskCore::maybeSettle() {
    // pending_ == 0. Cancel intent wins unless an op already recorded Error/Cancelled.
    if (cancel_.cancelled()) {
        settle(SmbTaskStatus::Cancelled, "", static_cast<int>(SmbErrorCode::Cancelled));
        return;
    }
    const auto s = status_.load();
    if (s == SmbTaskStatus::Error || s == SmbTaskStatus::Cancelled) {
        bool expected = false;
        if (settled_.compare_exchange_strong(expected, true)) {
            {
                std::lock_guard<std::mutex> lk(stateMutex_);
                endedAt_ = nowMs();
            }
            notifyListeners();
            done_.set_value();
        }
        return;
    }
    settle(SmbTaskStatus::Success, "", static_cast<int>(SmbErrorCode::Unknown));
}

void SmbTaskCore::settle(SmbTaskStatus finalStatus, const std::string& error, int code) {
    // Cancellation intent beats a late Success attempt (closes cancel/maybeSettle race).
    SmbTaskStatus status = finalStatus;
    std::string msg = error;
    int errCode = code;
    if (status == SmbTaskStatus::Success && cancel_.cancelled()) {
        status = SmbTaskStatus::Cancelled;
        msg.clear();
        errCode = static_cast<int>(SmbErrorCode::Cancelled);
    }

    // First settle wins — later op throws or emitStatus cannot override terminal state.
    bool expected = false;
    if (!settled_.compare_exchange_strong(expected, true)) return;
    SMB_LOG("Task settle id=%s kind=%s status=%s code=%d%s%s", id_.c_str(), operationName(kind_), statusName(status), errCode,
            msg.empty() ? "" : " error=", msg.empty() ? "" : msg.c_str());

    {
        std::lock_guard<std::mutex> lk(stateMutex_);
        status_ = status;
        if (!msg.empty()) errorMessage_ = msg;
        errorCode_ = errCode;
        endedAt_ = nowMs();
        updatedAt_ = endedAt_;
        if (status == SmbTaskStatus::Success) {
            if (isDeterminate(kind_) && totalBytes_.load() > 0) bytesDone_ = totalBytes_.load();
            progress_ = isDeterminate(kind_) ? 1.0 : 0.0;
            etaSeconds_ = 0.0;
        }
    }
    notifyListeners();
    done_.set_value();
}

}  // namespace react_native_smb
