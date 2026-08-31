#pragma once

#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <jsi/jsi.h>

#include "CancellationToken.hpp"
#include "SmbEnums.hpp"
#include "SmbTypes.hpp"

namespace react_native_smb {

class OperatorBase;

namespace jsi = facebook::jsi;

// Thread-safe state, result storage, and subscriber fan-out. No HybridObject,
// no pool orchestration.
class SmbTaskCore {
   public:
    SmbTaskCore(std::string id, SmbOperatorKind kind);
    ~SmbTaskCore();

    void cancel();
    SmbTaskState getState() const;
    std::string getId() const { return id_; }
    SmbTaskStatus getStatus() const { return status_.load(); }
    bool isSettled() const { return settled_.load(std::memory_order_acquire); }
    CancellationToken token() const { return cancel_; }

    void setPaths(std::string source, std::string destination);
    void addExpectedBytes(int64_t bytes);

    void onOpStatus(size_t opIndex, SmbTaskStatus s, const std::string& error, int code);
    void onOpProgress(size_t opIndex, double done, double total);

    void publishResult(std::any value, std::function<jsi::Value(jsi::Runtime&, const std::any&)> converter);
    jsi::Value getResultValue(jsi::Runtime& runtime) const;

    std::string subscribe(SnapshotListener listener);
    void unsubscribe(const std::string& id);

    std::future<void> completionFuture();

    void markPending();
    void markRunning();

   protected:
    struct OpProgress {
        int64_t done{0};
        int64_t total{0};
        SmbTaskStatus status{SmbTaskStatus::Idle};
    };

    std::vector<std::unique_ptr<OperatorBase>> ops_;
    std::vector<OpProgress> opProgress_;
    std::atomic<size_t> pending_{0};
    CancellationToken cancel_;

    void onOpFinished();
    void settle(SmbTaskStatus finalStatus, const std::string& error, int code);

    static int64_t nowMs();

    mutable std::mutex stateMutex_;

   private:
    std::string id_;
    SmbOperatorKind kind_{SmbOperatorKind::Initialize};
    std::atomic<SmbTaskStatus> status_{SmbTaskStatus::Idle};
    std::atomic<double> progress_{0.0};
    std::atomic<int> errorCode_{0};
    std::string errorMessage_;
    std::string sourcePath_;
    std::string destinationPath_;
    std::atomic<int64_t> bytesDone_{0};
    std::atomic<int64_t> totalBytes_{0};
    double bytesPerSecond_{0.0};
    double etaSeconds_{0.0};
    int64_t startedAt_{0};
    int64_t updatedAt_{0};
    int64_t endedAt_{0};

    std::promise<void> done_;
    std::atomic<bool> settled_{false};

    std::map<std::string, SnapshotListener> listeners_;
    mutable std::mutex listenersMutex_;
    std::atomic<size_t> nextListenerId_{1};

    std::any resultValue_;
    std::function<jsi::Value(jsi::Runtime&, const std::any&)> resultConverter_;

    void writeToSnapshot(SmbTaskState& out) const;
    void notifyListeners();
    void recomputeAggregateLocked();
    void maybeSettle();
};

}  // namespace react_native_smb