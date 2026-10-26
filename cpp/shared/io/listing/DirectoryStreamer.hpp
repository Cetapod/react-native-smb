#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../../core/CancellationToken.hpp"
#include "../../util/SmbErrorMapper.hpp"
#include "../SmbSecurityQuery.hpp"
#include "BatchPolicy.hpp"
#include "DirQuery.hpp"

struct smb2_context;

namespace react_native_smb {

class SmbConnectionManager;

namespace listing {

struct DirectoryStreamOptions {
    size_t minBatch{10};
    std::chrono::milliseconds maxDelay{100};
    // childCount / security-descriptor lookups in flight on the same connection.
    size_t maxConcurrentLookups{8};
    bool includeSecurityDescriptor{false};
};

// Lists one directory level incrementally on a single, exclusively held context.
//
// The listing is decoded batch by batch as QUERY_DIRECTORY replies arrive. Each
// entry is "ready" once its lookups (childCount for directories, optional
// security descriptor) finish; those lookups are pipelined on the same
// connection. Ready entries are pushed to the sink in server order according to
// BatchPolicy. All libsmb2 callbacks run on the run() thread, so no locking.
class DirectoryStreamer {
   public:
    using Sink = std::function<void(std::vector<SmbFileInfo>&&)>;

    struct Stats {
        size_t entries{0};
        size_t directories{0};
        size_t batches{0};
        std::optional<std::chrono::milliseconds> firstBatchAfter;
    };

    DirectoryStreamer(smb2_context* ctx, SmbConnectionManager& manager, std::string path, DirectoryStreamOptions options,
                      CancellationToken cancel, Sink sink);
    ~DirectoryStreamer();
    DirectoryStreamer(const DirectoryStreamer&) = delete;
    DirectoryStreamer& operator=(const DirectoryStreamer&) = delete;

    // Streams the whole directory. Returns normally when cancelled; throws
    // SmbException on failure (entries already emitted are then incomplete).
    void run();

    const Stats& stats() const { return stats_; }

   private:
    using Clock = std::chrono::steady_clock;

    struct Pending {
        SmbFileInfo info;
        int outstanding{0};  // unfinished lookups
    };
    enum class LookupKind { ChildCount, SecurityDescriptor };
    struct Lookup {
        LookupKind kind;
        uint64_t seq;
    };
    struct ChildCountOp {
        uint64_t seq{0};
        int64_t count{0};
        bool malformed{false};
        std::unique_ptr<DirQuery> query;
    };
    struct SecurityOp {
        uint64_t seq{0};
        SecurityDescriptorQuery query;
    };
    struct Failure {
        SmbErrorCode code;
        std::string message;
    };

    // Listing
    bool onListingChunk(const uint8_t* buffer, uint32_t length);
    void onListingDone(uint32_t status);

    // Lookups
    void launchLookups();
    void startChildCount(uint64_t seq);
    void startSecurityDescriptor(uint64_t seq);
    void onChildCountDone(ChildCountOp& op, uint32_t status);
    void onSecurityDone(SecurityOp& op);
    void completeLookup(uint64_t seq);
    void reapIdleOps();

    // Emission
    size_t readyCount();
    void flush(Clock::time_point now);

    // Control
    void pollOnce(Clock::time_point now);
    bool finished() const;
    bool opsIdle() const;
    void fail(SmbErrorCode code, std::string message);
    void stopAll();
    void invalidate();
    Pending* entryAt(uint64_t seq);

    smb2_context* ctx_;
    SmbConnectionManager& manager_;
    std::string path_;
    DirectoryStreamOptions options_;
    CancellationToken cancel_;
    Sink sink_;
    BatchPolicy policy_;

    std::unique_ptr<DirQuery> listing_;
    bool listingComplete_{false};

    std::deque<Pending> pending_;  // decoded, not yet emitted (server order)
    uint64_t emitted_{0};          // sequence number of pending_.front()
    size_t readyCursor_{0};        // pending_[0, readyCursor_) are ready

    std::deque<Lookup> lookups_;  // not yet started
    std::list<std::unique_ptr<ChildCountOp>> countOps_;
    std::list<std::unique_ptr<SecurityOp>> securityOps_;

    bool stopping_{false};
    bool cancelled_{false};
    std::optional<Failure> failure_;
    std::optional<Clock::time_point> drainDeadline_;
    Clock::time_point startedAt_;
    Clock::time_point lastActivity_;
    Stats stats_;
};

}  // namespace listing
}  // namespace react_native_smb
