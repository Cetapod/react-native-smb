#pragma once

#include <chrono>
#include <cstddef>
#include <optional>

namespace react_native_smb::listing {

// Decides when ready entries are pushed to subscribers:
//   - the first batch goes out as soon as anything is ready,
//   - then when at least `minBatch` entries are ready or `maxDelay` has passed,
//   - and always once the listing is complete.
// A flush always takes every ready entry.
class BatchPolicy {
   public:
    using Clock = std::chrono::steady_clock;

    BatchPolicy(size_t minBatch, std::chrono::milliseconds maxDelay) : minBatch_(minBatch == 0 ? 1 : minBatch), maxDelay_(maxDelay) {}

    bool shouldFlush(size_t ready, bool complete, Clock::time_point now) const {
        if (ready == 0) return false;
        if (complete || !lastFlush_) return true;
        return ready >= minBatch_ || now - *lastFlush_ >= maxDelay_;
    }

    // Latest time a pending flush may wait until; nullopt when nothing is waiting.
    std::optional<Clock::time_point> deadline(size_t ready) const {
        if (ready == 0 || !lastFlush_) return std::nullopt;
        return *lastFlush_ + maxDelay_;
    }

    void markFlushed(Clock::time_point now) { lastFlush_ = now; }

   private:
    size_t minBatch_;
    std::chrono::milliseconds maxDelay_;
    std::optional<Clock::time_point> lastFlush_;
};

}  // namespace react_native_smb::listing
