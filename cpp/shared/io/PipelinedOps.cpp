#include "PipelinedOps.hpp"

#include <poll.h>

#include <algorithm>
#include <cerrno>
#include <utility>

#include "../connection/SmbConnection.hpp"
#include "../util/SmbException.hpp"

namespace react_native_smb {

namespace {
constexpr auto kPollInterval = std::chrono::milliseconds(100);
constexpr auto kIdleTimeout = std::chrono::seconds(60);
// Long enough for cleanup (close, remove partial files) after a failure or cancel.
constexpr auto kDrainTimeout = std::chrono::seconds(5);
}  // namespace

PipelinedOps::PipelinedOps(smb2_context* ctx, SmbConnectionManager& manager, CancellationToken cancel, size_t maxInFlight,
                           std::string label)
    : ctx_(ctx), manager_(manager), cancel_(std::move(cancel)), maxInFlight_(std::max<size_t>(1, maxInFlight)), label_(std::move(label)) {}

PipelinedOps::~PipelinedOps() {
    // Pending callbacks point into this object: tear the transport down first.
    if (ctx_ && !inFlight_.empty()) invalidate();
}

void PipelinedOps::submit(Issue issue, Done done, bool cleanup) {
    auto op = std::make_unique<Op>();
    op->owner = this;
    op->issue = std::move(issue);
    op->done = std::move(done);
    op->cleanup = cleanup;
    queued_.push_back(std::move(op));
}

void PipelinedOps::fail(SmbErrorCode code, std::string message) {
    if (!failure_ && !cancelled_) failure_ = Failure{code, std::move(message)};
    stop();
}

void PipelinedOps::failFromErrno(int result, const std::string& message) {
    fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(result, 0, message)), message);
}

void PipelinedOps::run() {
    if (!ctx_) SmbException::raise(SmbErrorCode::NotConnected, label_ + ": SMB context is not connected");
    lastActivity_ = Clock::now();

    for (;;) {
        if (!stopping_ && cancel_.cancelled()) {
            cancelled_ = true;
            stop();
        }
        launch();
        if (queued_.empty() && inFlight_.empty()) break;

        const auto now = Clock::now();
        if (stopping_ && drainDeadline_ && now > *drainDeadline_) {
            invalidate();
            break;
        }
        if (now - lastActivity_ > kIdleTimeout) {
            fail(SmbErrorCode::TimedOut, label_ + ": no reply from the server");
            invalidate();
            break;
        }

        pollOnce();
        if (!ctx_) break;
    }

    if (failure_) throw SmbException(failure_->code, failure_->message);
}

void PipelinedOps::launch() {
    while (ctx_ && !queued_.empty() && inFlight_.size() < maxInFlight_) {
        std::unique_ptr<Op> op = std::move(queued_.front());
        queued_.pop_front();
        if (stopping_ && !op->cleanup) continue;

        Op* raw = op.get();
        inFlight_.push_back(std::move(op));
        const int result = raw->issue(ctx_, &PipelinedOps::onReply, raw);
        if (result != 0) {
            inFlight_.pop_back();
            failFromErrno(result, label_ + ": could not queue request: " + smb2_get_error(ctx_));
        }
    }
}

void PipelinedOps::onReply(smb2_context* /*smb2*/, int status, void* commandData, void* privateData) {
    auto* raw = static_cast<Op*>(privateData);
    PipelinedOps& self = *raw->owner;
    self.lastActivity_ = Clock::now();

    auto it = std::find_if(self.inFlight_.begin(), self.inFlight_.end(), [raw](const std::unique_ptr<Op>& op) { return op.get() == raw; });
    if (it == self.inFlight_.end()) return;
    std::unique_ptr<Op> op = std::move(*it);
    self.inFlight_.erase(it);

    try {
        if (op->done) op->done(status, commandData);
    } catch (const SmbException& e) {
        self.fail(e.code(), e.what());
    } catch (const std::exception& e) {
        self.fail(SmbErrorCode::Unknown, self.label_ + ": " + e.what());
    } catch (...) {
        // Never unwind through libsmb2's C callback stack.
        self.fail(SmbErrorCode::Unknown, self.label_ + ": unknown error");
    }
}

void PipelinedOps::stop() {
    if (stopping_) return;
    stopping_ = true;
    drainDeadline_ = Clock::now() + kDrainTimeout;
}

void PipelinedOps::pollOnce() {
    pollfd pfd{};
    pfd.fd = smb2_get_fd(ctx_);
    pfd.events = static_cast<short>(smb2_which_events(ctx_));
    if (pfd.fd < 0) {
        fail(SmbErrorCode::NotConnected, label_ + ": connection lost");
        invalidate();
        return;
    }

    const int result = ::poll(&pfd, 1, static_cast<int>(kPollInterval.count()));
    if (result < 0) {
        if (errno == EINTR) return;
        fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromErrno(errno)), label_ + ": poll failed");
        invalidate();
        return;
    }
    if (result == 0) return;

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        fail(SmbErrorCode::NotConnected, label_ + ": socket closed");
        invalidate();
        return;
    }
    if (smb2_service(ctx_, pfd.revents) < 0) {
        const std::string message = label_ + ": " + smb2_get_error(ctx_);
        fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(-1, smb2_get_nterror(ctx_), message, SmbErrorCode::NotConnected)),
             message);
        invalidate();
    }
}

void PipelinedOps::invalidate() {
    stop();
    if (!ctx_) return;
    ctx_ = nullptr;
    // Destroys the context; libsmb2 fires every pending callback with SHUTDOWN.
    manager_.invalidateContext();
    inFlight_.clear();
    queued_.clear();
}

}  // namespace react_native_smb
