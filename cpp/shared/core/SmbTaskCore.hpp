#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "CancellationToken.hpp"
#include "SmbEnums.hpp"
#include "SmbTaskResult.hpp"
#include "SmbTypes.hpp"

namespace react_native_smb {

// Thread-safe state, result storage, and subscriber fan-out. No HybridObject,
// no pool orchestration, no JSI.
class SmbTaskCore {
   public:
    SmbTaskCore(std::string id, SmbOperatorKind kind);
    ~SmbTaskCore();

    void cancel();
    SmbTaskState getState() const;
    std::string getId() const { return id_; }
    SmbOperatorKind getKind() const { return kind_; }
    SmbTaskStatus getStatus() const { return status_.load(); }
    bool isSettled() const { return settled_.load(std::memory_order_acquire); }
    CancellationToken token() const { return cancel_; }

    void setPaths(std::string source, std::string destination);
    void addExpectedBytes(int64_t bytes);

    void onOpStatus(SmbTaskStatus s, const std::string& error, int code);
    void onOpProgress(double done, double total);

    void publishResult(SmbTaskResult value);
    SmbTaskResult publishedResult() const;

    std::string subscribe(SnapshotListener listener);
    void unsubscribe(const std::string& id);

    void markPending();
    void markRunning();

   protected:
    CancellationToken cancel_;

    void settle(SmbTaskStatus finalStatus, const std::string& error, int code);
    // Settles from the operator's outcome once it has returned.
    void maybeSettle();

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
    int64_t opDone_{0};
    int64_t opTotal_{0};

    std::atomic<bool> settled_{false};

    std::map<std::string, SnapshotListener> listeners_;
    mutable std::mutex listenersMutex_;
    std::atomic<size_t> nextListenerId_{1};

    SmbTaskResult resultValue_;

    void writeToSnapshot(SmbTaskState& out) const;
    void notifyListeners();
    void recomputeAggregateLocked();
};

}  // namespace react_native_smb
