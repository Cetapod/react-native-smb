#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "../ReactNativeSmb.hpp"  // SmbFileInfo

namespace react_native_smb {

// Append-only stream of listing entries owned by a task. It is also the single
// storage of the final listing result.
//
// Subscribers first receive everything appended so far as one batch, then each
// live batch. Listeners run under the lock so a replay can never be overtaken
// by a later batch; JS listeners only enqueue onto the JS thread, so this cannot
// re-enter.
class EntryStream {
   public:
    using Batch = std::vector<SmbFileInfo>;
    using Listener = std::function<void(const Batch&)>;

    void append(Batch batch) {
        if (batch.empty()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, listener] : listeners_) notify(listener, batch);
        entries_.insert(entries_.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
    }

    std::string subscribe(Listener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!entries_.empty()) notify(listener, entries_);
        std::string id = "ent_" + std::to_string(nextId_++);
        listeners_.emplace(id, std::move(listener));
        return id;
    }

    void unsubscribe(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.erase(id);
    }

    template <typename Fn>
    auto read(Fn&& fn) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fn(entries_);
    }

   private:
    static void notify(const Listener& listener, const Batch& batch) {
        try {
            if (listener) listener(batch);
        } catch (...) {
        }
    }

    mutable std::mutex mutex_;
    Batch entries_;
    std::map<std::string, Listener> listeners_;
    size_t nextId_{1};
};

}  // namespace react_native_smb
