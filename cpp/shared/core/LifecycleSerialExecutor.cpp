#include "LifecycleSerialExecutor.hpp"

#include <stdexcept>
#include <utility>

namespace react_native_smb {

LifecycleSerialExecutor::LifecycleSerialExecutor() : worker_([this] { workerLoop(); }) {}

LifecycleSerialExecutor::~LifecycleSerialExecutor() { shutdown(); }

void LifecycleSerialExecutor::post(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            throw std::runtime_error("LifecycleSerialExecutor is shutting down");
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void LifecycleSerialExecutor::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            // Already shutting down / shut down; still join if joinable.
        } else {
            stopping_ = true;
        }
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void LifecycleSerialExecutor::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (tasks_.empty()) {
                // stopping_ is true and the queue is drained.
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (...) {
            // Task runners are expected to catch internally; swallow to keep worker alive.
        }
    }
}

}  // namespace react_native_smb
