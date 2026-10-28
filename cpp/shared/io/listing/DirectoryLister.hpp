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

struct DirectoryListOptions {
    // Nested levels to expand into `children`: 0 = this level only, -1 = unlimited.
    int maxDepth{0};
    size_t minBatch{10};
    std::chrono::milliseconds maxDelay{100};
    // Directory expansions, childCount and security-descriptor lookups in flight
    // on the same connection.
    size_t maxConcurrentLookups{8};
    bool includeSecurityDescriptor{false};
};

// Lists a directory (optionally a tree) incrementally on a single, exclusively
// held context.
//
// QUERY_DIRECTORY replies are decoded as they arrive. Every entry carries a
// count of unfinished lookups: expanding a directory within maxDepth, counting
// a directory at the depth limit, and the optional security descriptor. An
// expanded directory also waits for each of its children; once done its
// children move into `children` and childCount is their number, so every
// directory is opened exactly once. Lookups are pipelined on the connection.
//
// Top-level entries are pushed to the sink in server order according to
// BatchPolicy, each with its complete subtree. All libsmb2 callbacks run on the
// run() thread, so no locking.
class DirectoryLister {
   public:
    using Sink = std::function<void(std::vector<SmbFileInfo>&&)>;

    struct Stats {
        size_t entries{0};
        size_t directories{0};
        size_t listedDirectories{0};
        size_t batches{0};
        std::optional<std::chrono::milliseconds> firstBatchAfter;
    };

    DirectoryLister(smb2_context* ctx, SmbConnectionManager& manager, std::string path, DirectoryListOptions options, CancellationToken cancel,
                    Sink sink);
    ~DirectoryLister();
    DirectoryLister(const DirectoryLister&) = delete;
    DirectoryLister& operator=(const DirectoryLister&) = delete;

    // Lists everything. Returns normally when cancelled; throws SmbException on
    // failure (entries already emitted are then incomplete).
    void run();

    const Stats& stats() const { return stats_; }

   private:
    using Clock = std::chrono::steady_clock;

    struct Node {
        SmbFileInfo info;
        Node* parent{nullptr};
        int level{0};        // top-level entries are 0
        int outstanding{0};  // unfinished lookups + unfinished children
        bool expanded{false};
        std::vector<std::unique_ptr<Node>> children;  // server order
    };
    enum class LookupKind { Expand, ChildCount, SecurityDescriptor };
    struct Lookup {
        LookupKind kind;
        Node* node;
    };
    // An expansion or child count: one DirQuery whose replies are decoded or counted.
    struct DirOp {
        LookupKind kind{LookupKind::Expand};
        Node* node{nullptr};
        int64_t count{0};
        std::unique_ptr<DirQuery> query;
    };
    struct SecurityOp {
        Node* node{nullptr};
        SecurityDescriptorQuery query;
    };
    struct Failure {
        SmbErrorCode code;
        std::string message;
    };

    // Listing
    bool decodeEntries(Node* parent, const uint8_t* buffer, uint32_t length);
    void onListingDone(uint32_t status);

    // Lookups
    void launchLookups();
    void startDirOp(const Lookup& lookup);
    void startSecurityDescriptor(Node* node);
    void onDirOpDone(DirOp& op, uint32_t status);
    void onSecurityDone(SecurityOp& op);
    void release(Node* node);
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

    smb2_context* ctx_;
    SmbConnectionManager& manager_;
    std::string path_;
    DirectoryListOptions options_;
    CancellationToken cancel_;
    Sink sink_;
    BatchPolicy policy_;

    std::unique_ptr<DirQuery> listing_;
    bool listingComplete_{false};

    std::deque<std::unique_ptr<Node>> roots_;  // top-level entries not yet emitted (server order)
    size_t readyCursor_{0};                    // roots_[0, readyCursor_) are ready

    std::deque<Lookup> lookups_;  // not yet started; subtree lookups go first
    std::list<std::unique_ptr<DirOp>> dirOps_;
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
