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
// Delivery is notify-and-drain: appends wake each subscriber at most once until
// that subscriber drains. A busy JS thread therefore queues one pending signal
// per subscriber and receives every entry appended since its last drain as a
// single coalesced batch. JS drains once right after subscribe so late
// subscribers still get a replay of entries already stored. Signals run outside
// the lock and must not re-enter EntryStream synchronously.
class EntryStream {
   public:
    using Batch = std::vector<SmbFileInfo>;
    using Signal = std::function<void()>;

    void append(Batch batch) {
        if (batch.empty()) return;
        std::vector<Signal> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            entries_.insert(entries_.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
            for (auto& [id, sub] : subscribers_) {
                if (sub.scheduled) continue;
                sub.scheduled = true;
                if (sub.signal) pending.push_back(sub.signal);
            }
        }
        for (const auto& signal : pending) notify(signal);
    }

    std::string subscribe(Signal signal) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string id = "ent_" + std::to_string(nextId_++);
        subscribers_.emplace(id, Subscriber{std::move(signal), 0, false});
        return id;
    }

    void unsubscribe(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.erase(id);
    }

    Batch drain(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(id);
        if (it == subscribers_.end()) return {};
        Subscriber& sub = it->second;
        Batch out;
        if (sub.cursor < entries_.size()) {
            out.assign(entries_.begin() + static_cast<std::ptrdiff_t>(sub.cursor), entries_.end());
            sub.cursor = entries_.size();
        }
        sub.scheduled = false;
        return out;
    }

    template <typename Fn>
    auto read(Fn&& fn) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return fn(entries_);
    }

   private:
    struct Subscriber {
        Signal signal;
        size_t cursor{0};
        bool scheduled{false};
    };

    static void notify(const Signal& signal) {
        try {
            if (signal) signal();
        } catch (...) {
        }
    }

    mutable std::mutex mutex_;
    Batch entries_;
    std::map<std::string, Subscriber> subscribers_;
    size_t nextId_{1};
};

}  // namespace react_native_smb
