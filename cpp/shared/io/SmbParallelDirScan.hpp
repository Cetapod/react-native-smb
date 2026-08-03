#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../ReactNativeSmb.hpp"
#include "../connection/PoolTypes.hpp"
#include "../connection/SmbConnectionPool.hpp"
#include "../core/CancellationToken.hpp"
#include "../core/SmbEnums.hpp"
#include "SmbDirScan.hpp"

namespace react_native_smb {

// Bounded-parallel recursive listing. Each directory scan borrows a Metadata
// pool slot. Entry order within each directory matches readdir().
inline std::vector<SmbFileInfo> listDirectoryParallel(SmbConnectionPool& pool, const std::string& taskId, const std::string& path, int maxDepth,
                                                      const CancellationToken& cancel) {
    struct WorkItem {
        std::string path;
        int maxDepth{0};
        std::vector<SmbFileInfo>* out{nullptr};
        bool isRoot{false};
    };

    std::vector<SmbFileInfo> root;
    std::mutex mu;
    std::condition_variable cv;
    std::deque<WorkItem> queue;
    size_t pending = 0;
    std::exception_ptr rootError;
    bool stop = false;

    auto enqueueLocked = [&](WorkItem item) {
        queue.push_back(std::move(item));
        ++pending;
    };

    auto drainQueueLocked = [&] {
        const size_t n = queue.size();
        queue.clear();
        if (n >= pending)
            pending = 0;
        else
            pending -= n;
    };

    auto finishItemLocked = [&] {
        if (pending > 0) --pending;
        if (pending == 0 || rootError || stop) cv.notify_all();
    };

    {
        std::lock_guard<std::mutex> lock(mu);
        enqueueLocked(WorkItem{path, maxDepth, &root, true});
    }

    const size_t workerCount = std::max<size_t>(1, pool.maxConnections() > 0 ? pool.maxConnections() - 1 : 1);
    std::vector<std::thread> workers;
    workers.reserve(workerCount);

    for (size_t i = 0; i < workerCount; ++i) {
        workers.emplace_back([&] {
            for (;;) {
                WorkItem item;
                {
                    std::unique_lock<std::mutex> lock(mu);
                    cv.wait(lock, [&] { return stop || rootError || !queue.empty() || pending == 0; });
                    if (rootError || pending == 0) return;
                    if (stop) {
                        drainQueueLocked();
                        if (pending == 0) cv.notify_all();
                        return;
                    }
                    if (queue.empty()) continue;
                    item = std::move(queue.front());
                    queue.pop_front();
                }

                auto abortFromWorker = [&] {
                    std::lock_guard<std::mutex> lock(mu);
                    stop = true;
                    drainQueueLocked();
                    finishItemLocked();
                };

                if (cancel.cancelled()) {
                    abortFromWorker();
                    return;
                }

                try {
                    auto handle = pool.requestContext(AcquireMode::Metadata, SmbOperatorKind::ListDirectory, taskId);
                    std::vector<SmbFileInfo> files =
                        handle.submitSync([&](smb2_context* ctx) { return scanDirectoryOnCtx(ctx, item.path, cancel); });

                    if (cancel.cancelled()) {
                        abortFromWorker();
                        return;
                    }

                    *item.out = std::move(files);

                    if (item.maxDepth != 0) {
                        const int newMaxDepth = (item.maxDepth > 0) ? item.maxDepth - 1 : item.maxDepth;
                        std::lock_guard<std::mutex> lock(mu);
                        if (!stop && !rootError) {
                            for (auto& child : *item.out) {
                                if (!child.isDirectory) continue;
                                enqueueLocked(WorkItem{child.path, newMaxDepth, &child.children, false});
                            }
                            cv.notify_all();
                        }
                    }
                } catch (...) {
                    if (cancel.cancelled()) {
                        abortFromWorker();
                        return;
                    }
                    if (item.isRoot) {
                        std::lock_guard<std::mutex> lock(mu);
                        if (!rootError) rootError = std::current_exception();
                        drainQueueLocked();
                        finishItemLocked();
                        return;
                    }
                    if (item.out) item.out->clear();
                }

                {
                    std::lock_guard<std::mutex> lock(mu);
                    finishItemLocked();
                }
            }
        });
    }

    // Poll cancel so idle waiters are woken even if no pool/assign notify fires.
    while (true) {
        std::unique_lock<std::mutex> lock(mu);
        if (pending == 0 || rootError) break;
        if (cancel.cancelled()) {
            stop = true;
            drainQueueLocked();
            cv.notify_all();
            cv.wait(lock, [&] { return pending == 0 || rootError; });
            break;
        }
        cv.wait_for(lock, std::chrono::milliseconds(50), [&] { return pending == 0 || rootError || cancel.cancelled(); });
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    if (rootError) std::rethrow_exception(rootError);
    if (cancel.cancelled()) return {};
    return root;
}

}  // namespace react_native_smb
