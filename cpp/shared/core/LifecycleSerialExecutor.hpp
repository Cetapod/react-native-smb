#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace react_native_smb {

// Single-worker FIFO queue for connection lifecycle tasks (initialize / connect /
// connectShare / disconnect). Keeps those transitions off Nitro's shared
// Promise::async pool so bulk Metadata work cannot starve logout/reconnect.
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
