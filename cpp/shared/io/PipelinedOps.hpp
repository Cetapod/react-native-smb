#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "../core/CancellationToken.hpp"
#include "../util/SmbErrorMapper.hpp"

namespace react_native_smb {

class SmbConnectionManager;

// Runs async libsmb2 requests on an exclusively held context, up to
// `maxInFlight` at a time. Each submitted operation queues exactly one request;
// its `done` handler runs on the run() thread and may submit follow-up
// operations, which is how callers express ordering (e.g. rmdir after the
// folder's contents). Single-threaded: no locking.
//
// The first failure (or a cancel) stops new operations; requests already sent
// are drained. Operations submitted as `cleanup` still run while stopping, so
// partial results can be closed or removed.
class PipelinedOps {
   public:
    // Queues one request with the given callback; returns 0 when queued.
    using Issue = std::function<int(smb2_context* ctx, smb2_command_cb cb, void* cbData)>;
    // Receives the request's status and reply (libsmb2's callback arguments).
    using Done = std::function<void(int status, void* commandData)>;

    PipelinedOps(smb2_context* ctx, SmbConnectionManager& manager, CancellationToken cancel, size_t maxInFlight, std::string label);
    ~PipelinedOps();
    PipelinedOps(const PipelinedOps&) = delete;
    PipelinedOps& operator=(const PipelinedOps&) = delete;

    void submit(Issue issue, Done done, bool cleanup = false);

    // Records the first failure and stops new (non-cleanup) operations.
    void fail(SmbErrorCode code, std::string message);
    // Fails with the mapping of a high-level libsmb2 result (negative errno).
    void failFromErrno(int result, const std::string& message);
    bool stopping() const { return stopping_; }

    // Runs until nothing is queued or in flight. Returns normally when
    // cancelled; throws SmbException on failure.
    void run();

    smb2_context* ctx() const { return ctx_; }

   private:
    using Clock = std::chrono::steady_clock;

    struct Op {
        PipelinedOps* owner = nullptr;
        Issue issue;
        Done done;
        bool cleanup = false;
    };
    struct Failure {
        SmbErrorCode code;
        std::string message;
    };

    static void onReply(smb2_context* smb2, int status, void* commandData, void* privateData);
    void launch();
    void stop();
    void pollOnce();
    void invalidate();

    smb2_context* ctx_;
    SmbConnectionManager& manager_;
    CancellationToken cancel_;
    size_t maxInFlight_;
    std::string label_;

    std::deque<std::unique_ptr<Op>> queued_;
    std::list<std::unique_ptr<Op>> inFlight_;

    bool stopping_{false};
    bool cancelled_{false};
    std::optional<Failure> failure_;
    std::optional<Clock::time_point> drainDeadline_;
    Clock::time_point lastActivity_;
};

}  // namespace react_native_smb
