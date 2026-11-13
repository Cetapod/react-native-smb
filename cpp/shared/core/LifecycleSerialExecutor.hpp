#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace react_native_smb {

// Single-worker FIFO queue for connection lifecycle tasks (initialize / connect /
// connectShare / disconnect). They need no pool slot, so they never queue behind
// bulk work waiting for one; the task runs inline on this worker.
class LifecycleSerialExecutor {
   public:
    LifecycleSerialExecutor();
    ~LifecycleSerialExecutor();

    LifecycleSerialExecutor(const LifecycleSerialExecutor&) = delete;
    LifecycleSerialExecutor& operator=(const LifecycleSerialExecutor&) = delete;

    // Enqueue work. Throws if shutdown has started.
    void post(std::function<void()> task);

    // Stop accepting work, drain already-queued tasks, then join the worker.
    // Idempotent and safe to call from the HybridSMB destructor after taskRuns wait.
    void shutdown();

   private:
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    bool stopping_{false};
    std::thread worker_;
};

}  // namespace react_native_smb
