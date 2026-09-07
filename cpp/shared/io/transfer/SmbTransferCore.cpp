#include "SmbTransferCore.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>

#include <poll.h>
#include <smb2/libsmb2.h>

#include "../../connection/SmbConnection.hpp"
#include "../../util/SmbErrorMapper.hpp"

namespace react_native_smb::detail {
namespace {
constexpr int64_t kProgressMinBytes = 256 * 1024;
constexpr double kProgressMinFraction = 0.005;
constexpr uint32_t kAsyncChunkSizeCap = 256 * 1024;
constexpr auto kTransferIdleTimeout = std::chrono::minutes(5);

bool shouldEmitProgress(int64_t current, int64_t total, int64_t& lastEmitted) {
    if (total <= 0) {
        return false;
    }
    if (current - lastEmitted >= kProgressMinBytes ||
        static_cast<double>(current - lastEmitted) / static_cast<double>(total) >= kProgressMinFraction) {
        lastEmitted = current;
        return true;
    }
    return false;
}
}  // namespace

void TransferCore::emitProgress(int64_t current, int64_t total) {
    if (progressHandler && current < total && shouldEmitProgress(current, total, lastProgressBytes)) {
        progressHandler(static_cast<double>(current), static_cast<double>(total));
    }
}

SmbErrorCode mapSmbResult(int result, const std::string& message) {
    int mapped = SmbErrorMapper::fromErrnoResult(result);
    if (mapped == static_cast<int>(SmbErrorCode::Unknown)) {
        mapped = SmbErrorMapper::fromErrnoOrMessage(0, message);
    }
    if (mapped == static_cast<int>(SmbErrorCode::Unknown)) {
        mapped = static_cast<int>(SmbErrorCode::Io);
    }
    return static_cast<SmbErrorCode>(mapped);
}

SmbErrorCode mapErrnoOrIo(int error) {
    const int mapped = SmbErrorMapper::fromErrno(error);
    return mapped == static_cast<int>(SmbErrorCode::Unknown) ? SmbErrorCode::Io : static_cast<SmbErrorCode>(mapped);
}

uint32_t capAsyncChunkSize(uint32_t negotiatedSize) {
    return std::min(negotiatedSize == 0 ? 1024U * 1024U : negotiatedSize, kAsyncChunkSizeCap);
}

[[noreturn]] void raiseTransferError(SmbErrorCode code, const std::string& message) {
    if (code != SmbErrorCode::Unknown) {
        SmbException::raise(code, message);
    }
    SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, message), message);
}

PollLoopResult runTransfer(TransferCore& core, SmbConnectionManager& manager, const std::function<void()>& initialFill,
                           const std::function<bool()>& done, const std::function<bool()>& stuck,
                           const std::function<void()>& markCancelled, const std::function<void()>& clearHandles,
                           const std::string& failurePrefix) {
    initialFill();
    using clock = std::chrono::steady_clock;
    auto lastProgressAt = clock::now();
    int64_t lastSeenBytes = core.completedBytes;
    PollLoopResult result = PollLoopResult::Done;
    pollfd pfd{};
    while (!done() && !stuck()) {
        if (core.cancel.cancelled() && !core.failure.errored) {
            markCancelled();
        }
        if (done() || stuck()) {
            break;
        }
        if (core.completedBytes != lastSeenBytes) {
            lastSeenBytes = core.completedBytes;
            lastProgressAt = clock::now();
        } else if (clock::now() - lastProgressAt > kTransferIdleTimeout) {
            result = PollLoopResult::TimedOut;
            break;
        }
        pfd.fd = smb2_get_fd(core.ctx);
        pfd.events = smb2_which_events(core.ctx);
        pfd.revents = 0;
        if (pfd.fd < 0 || pfd.events == 0) {
            ::poll(nullptr, 0, 10);
            continue;
        }
        const int pollResult = ::poll(&pfd, 1, 100);
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = PollLoopResult::PollFailed;
            break;
        }
        if (pollResult > 0 && smb2_service(core.ctx, pfd.revents) < 0) {
            result = PollLoopResult::PollFailed;
            break;
        }
    }
    if (stuck()) {
        core.failure.fail(SmbErrorCode::Io, failurePrefix + ": I/O pipeline stalled (unrecoverable short transfer)");
    }
    if (result == PollLoopResult::TimedOut && !core.failure.errored) {
        if (core.cancel.cancelled())
            markCancelled();
        else {
            core.failure.fail(SmbErrorCode::TimedOut, failurePrefix + ": transfer timed out (no progress)");
        }
    }
    if (result == PollLoopResult::PollFailed && !core.failure.errored) {
        core.failure.fail(SmbErrorCode::Io, failurePrefix + ": poll/service: " + smb2_get_error(core.ctx));
    }
    if (result != PollLoopResult::Done) {
        // The manager owns transport teardown; never unwind callback stack state first.
        manager.invalidateContext();
        core.ctx = nullptr;
        clearHandles();
    }
    return result;
}

}  // namespace react_native_smb::detail
