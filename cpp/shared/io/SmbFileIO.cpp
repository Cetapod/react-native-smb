#include "SmbFileIO.hpp"  // sibling

#include <fcntl.h>
#include <poll.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "../connection/SmbConnection.hpp"
#include "../core/CancellationToken.hpp"
#include "../util/SmbErrorMapper.hpp"
#include "../util/SmbException.hpp"
#include "../util/SmbLog.hpp"
#include "SmbPathUtil.hpp"

namespace react_native_smb {

// Progress coalescing: emit only when >=256 KiB or >=0.5% have elapsed since
// the last emission.
static constexpr int64_t kProgressMinBytes = 256 * 1024;  // 256 KiB
static constexpr double kProgressMinFraction = 0.005;     // 0.5%

// Keep the async pipeline responsive to cancellation. The slot can only be
// returned after already-submitted SMB requests finish, so cap per-request
// bytes instead of tearing down the connection on cancel.
static constexpr uint32_t kAsyncChunkSizeCap = 256 * 1024;  // 256 KiB

inline bool shouldEmitProgress(int64_t current, int64_t total, int64_t& lastEmitted) {
    if (total <= 0) return false;
    if (current - lastEmitted >= kProgressMinBytes) {
        lastEmitted = current;
        return true;
    }
    double delta = static_cast<double>(current - lastEmitted) / static_cast<double>(total);
    if (delta >= kProgressMinFraction) {
        lastEmitted = current;
        return true;
    }
    return false;
}

inline uint32_t capAsyncChunkSize(uint32_t negotiatedSize) {
    if (negotiatedSize == 0) negotiatedSize = 1024 * 1024;
    return std::min(negotiatedSize, kAsyncChunkSizeCap);
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

SmbErrorCode mapErrnoOrIo(int err) {
    const int mapped = SmbErrorMapper::fromErrno(err);
    return mapped == static_cast<int>(SmbErrorCode::Unknown) ? SmbErrorCode::Io : static_cast<SmbErrorCode>(mapped);
}


// ═══════════════════════════════════════════════════════════════════════════════
// Async pipelined file I/O
//
// These use smb2_pread_async / smb2_pwrite_async to keep up to
// kMaxInFlight chunks on the wire simultaneously, exploiting SMB2
// credit-based pipelining for significantly higher throughput.
//
// Lifecycle (synchronous, on caller's Nitro worker thread under mutex):
//   1. Caller opens the remote file, computes file size & chunk size.
//   2. Caller kicks off the first batch of smb2_pread_async / pwrite_async.
//   3. Caller drives a local poll()/smb2_service() loop. Each
//      smb2_service() call fires readCb/writeCb on the same thread; those
//      callbacks copy bytes to/from the local file and submit the next
//      chunk.
//   4. The poll loop exits when all chunks completed / errored / cancelled.
//   5. Caller returns the byte count, or throws on error.
//
// Safety:
//   - State pointer passed as cb_data lives on the caller's stack frame
//     (the function does not return until the poll loop completes), so
//     callbacks always have a valid `this`.
//   - All callbacks run on the calling thread, while it holds
//     connectionMutex_, so libsmb2 sees a single-threaded user.
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

// Number of chunks kept in flight on a single smb2_context.
static constexpr int kMaxInFlight = 8;

// Progress coalescing thresholds. Avoid invoking the JS-side progress
// callback on every chunk completion — that callback crosses the JNI / JSI
// bridge and can back-pressure the transfer thread while connectionMutex_
// is held.
//   - Always emit when at least kProgressMinBytes have elapsed since the
//     last emission, OR
//   - the relative progress moved at least kProgressMinFraction (0.5%).
// Final 100% is emitted explicitly by the caller of the async functions.

// ─── Async pipelined read ────────────────────────────────────────────────────
//
// Slot lifecycle:
//   1. submitNextRead() picks an idle slot, fills (buf, baseOffset, requested,
//      received=0) and issues smb2_pread_async on the slot's full buffer.
//   2. readCb() may receive a SHORT READ (status < requested). When this
//      happens the slot stays busy: we re-submit smb2_pread_async on the
//      tail (buf + received, requested - received, baseOffset + received).
//      The slot is freed only when received == requested.
//   3. findSlot() locates a slot by range containment because the in-flight
//      buffer pointer may be in the middle of the slot's buffer (tail re-issue).

struct AsyncReadState {
    smb2_context* ctx = nullptr;
    smb2fh* fh = nullptr;
    FILE* outFile = nullptr;

    int64_t fileSize = 0;
    uint32_t chunkSize = 0;

    uint64_t nextOffset = 0;
    int64_t bytesCompleted = 0;
    int64_t lastEmittedBytes = 0;
    int inFlight = 0;
    bool errored = false;
    SmbErrorCode errorCode{SmbErrorCode::Unknown};
    std::string errorMsg;

    void fail(SmbErrorCode code, std::string msg) {
        if (errored) return;
        errored = true;
        errorCode = code;
        errorMsg = std::move(msg);
    }

    struct ChunkSlot {
        std::vector<uint8_t> buf;
        uint64_t baseOffset = 0;  // file offset corresponding to buf[0]
        uint32_t requested = 0;   // total bytes requested for this chunk
        uint32_t received = 0;    // bytes delivered so far (across re-submits)
        bool available = true;
    };
    ChunkSlot slots[kMaxInFlight];

    std::function<void(double, double)> progressHandler;
    CancellationToken cancel;
    std::string localPath;

    bool isDone() const { return errored || bytesCompleted >= fileSize; }

    void markCancelled() {
        fail(SmbErrorCode::Cancelled, "Download cancelled");
    }

    // Safety: every byte has been requested, nothing is in flight, all
    // slots are free, but bytesCompleted < fileSize. Without this guard,
    // a residual short-read deficit would leave the poll loop hanging.
    bool isStuck() const {
        if (errored || inFlight > 0) return false;
        if (nextOffset < static_cast<uint64_t>(fileSize)) return false;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (!slots[i].available) return false;
        }
        return bytesCompleted < fileSize;
    }

    // Locate the slot whose buffer range contains bufPtr (the in-flight
    // smb2 callback may point into the middle of the slot's buffer when
    // the slot is mid-recovery from a short read).
    int findSlot(const uint8_t* bufPtr) {
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].available) continue;
            const uint8_t* base = slots[i].buf.data();
            if (bufPtr >= base && bufPtr < base + slots[i].buf.size()) return i;
        }
        return -1;
    }

    void submitNextRead() {
        if (errored || nextOffset >= static_cast<uint64_t>(fileSize)) return;
        if (cancel.cancelled()) {
            markCancelled();
            return;
        }

        int slot = -1;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].available) {
                slot = i;
                break;
            }
        }
        if (slot < 0) return;

        uint64_t remaining = static_cast<uint64_t>(fileSize) - nextOffset;
        uint32_t len = static_cast<uint32_t>(std::min(static_cast<uint64_t>(chunkSize), remaining));

        slots[slot].buf.resize(len);
        slots[slot].baseOffset = nextOffset;
        slots[slot].requested = len;
        slots[slot].received = 0;
        slots[slot].available = false;

        uint64_t offset = nextOffset;
        nextOffset += len;

        int ret = smb2_pread_async(ctx, fh, slots[slot].buf.data(), len, offset, readCb, this);
        if (ret < 0) {
            const std::string msg = std::string("Download Failed: smb2_pread_async: ") + smb2_get_error(ctx);
            fail(mapSmbResult(ret, msg), msg);
            slots[slot].available = true;
            nextOffset -= len;
            return;
        }
        inFlight++;
    }

    static void readCb(smb2_context* smb2, int status, void* command_data, void* cb_data) {
        auto* st = static_cast<AsyncReadState*>(cb_data);
        st->inFlight--;

        auto* cbd = static_cast<smb2_read_cb_data*>(command_data);

        // ── Locate the slot for this completion (range-contains lookup) ──
        int slotIdx = (cbd ? st->findSlot(cbd->buf) : -1);

        // ── Error / already-errored path ──
        if (st->errored) {
            if (slotIdx >= 0) st->slots[slotIdx].available = true;
            return;
        }
        if (status < 0) {
            const std::string msg = std::string("Download Failed: read error: ") + smb2_get_error(smb2);
            st->fail(mapSmbResult(status, msg), msg);
            if (slotIdx >= 0) st->slots[slotIdx].available = true;
            return;
        }
        if (slotIdx < 0) {
            st->fail(SmbErrorCode::Io, "Download Failed: internal slot lookup failed");
            return;
        }

        auto& slot = st->slots[slotIdx];

        // File offset where THIS returned chunk starts (slot may already
        // hold `received` bytes from prior short-read recoveries).
        const uint64_t writeOffset = slot.baseOffset + slot.received;

        // ── Persist the received bytes to local file ──
        if (st->outFile && status > 0) {
            if (fseeko(st->outFile, static_cast<off_t>(writeOffset), SEEK_SET) != 0 || fwrite(cbd->buf, 1, static_cast<size_t>(status), st->outFile) != static_cast<size_t>(status)) {
                st->fail(mapErrnoOrIo(errno), "Download Failed: local file write error");
                slot.available = true;
                return;
            }
        }

        slot.received += static_cast<uint32_t>(status);
        st->bytesCompleted += status;

        // ── L1: short-read recovery ─────────────────────────────────────
        // If the server returned fewer bytes than requested for this slot,
        // re-submit the tail on the SAME slot without freeing it.
        if (slot.received < slot.requested) {
            if (status == 0) {
                st->fail(SmbErrorCode::Io, "Download Failed: unexpected EOF (zero-byte read)");
                slot.available = true;
                return;
            }
            uint32_t tailLen = slot.requested - slot.received;
            uint64_t tailOffset = slot.baseOffset + slot.received;
            int ret = smb2_pread_async(st->ctx, st->fh, slot.buf.data() + slot.received, tailLen, tailOffset, readCb, st);
            if (ret < 0) {
                const std::string msg = std::string("Download Failed: short-read recovery smb2_pread_async: ") + smb2_get_error(st->ctx);
                st->fail(mapSmbResult(ret, msg), msg);
                slot.available = true;
                return;
            }
            st->inFlight++;
            return;
        }

        // ── Slot fully satisfied ──
        slot.available = true;

        if (st->progressHandler && st->bytesCompleted < st->fileSize && shouldEmitProgress(st->bytesCompleted, st->fileSize, st->lastEmittedBytes)) {
            st->progressHandler(static_cast<double>(st->bytesCompleted), static_cast<double>(st->fileSize));
        }

        st->submitNextRead();
    }

    void cleanup() {
        if (outFile) {
            // Best-effort close during error cleanup — finalizeLocalFile() checks on the success path.
            fclose(outFile);
            outFile = nullptr;
        }
        if (fh && ctx) {
            smb2_close(ctx, fh);
            fh = nullptr;
        }
    }

    // Flush+close local file on success path. Returns false and sets errored on failure.
    bool finalizeLocalFile() {
        if (!outFile) return true;
        if (fflush(outFile) != 0) {
            const int err = errno;
            fclose(outFile);
            outFile = nullptr;
            fail(mapErrnoOrIo(err), std::string("Download Failed: fflush failed: ") + std::strerror(err));
            return false;
        }
        if (fclose(outFile) != 0) {
            const int err = errno;
            outFile = nullptr;
            fail(mapErrnoOrIo(err), std::string("Download Failed: fclose failed: ") + std::strerror(err));
            return false;
        }
        outFile = nullptr;
        return true;
    }

    bool closeRemote() {
        if (!(fh && ctx)) return true;
        const int r = smb2_close(ctx, fh);
        fh = nullptr;
        if (r < 0) {
            const std::string msg = std::string("Download Failed: smb2_close: ") + smb2_get_error(ctx);
            fail(mapSmbResult(r, msg), msg);
            return false;
        }
        return true;
    }
};

// ─── Async pipelined write ───────────────────────────────────────────────────
//
// Symmetric to AsyncReadState: a slot is kept busy until its full requested
// byte range has been acknowledged by the server. Short writes are recovered
// by re-issuing smb2_pwrite_async on the un-written tail of the same slot.

struct AsyncWriteState {
    smb2_context* ctx = nullptr;
    smb2fh* fh = nullptr;
    FILE* inFile = nullptr;

    int64_t fileSize = 0;
    uint32_t chunkSize = 0;

    uint64_t nextOffset = 0;
    int64_t bytesCompleted = 0;
    int64_t lastEmittedBytes = 0;
    int inFlight = 0;
    bool errored = false;
    SmbErrorCode errorCode{SmbErrorCode::Unknown};
    std::string errorMsg;

    void fail(SmbErrorCode code, std::string msg) {
        if (errored) return;
        errored = true;
        errorCode = code;
        errorMsg = std::move(msg);
    }

    struct ChunkSlot {
        std::vector<uint8_t> buf;
        uint64_t baseOffset = 0;
        uint32_t requested = 0;
        uint32_t written = 0;
        bool available = true;
    };
    ChunkSlot slots[kMaxInFlight];

    std::function<void(double, double)> progressHandler;
    CancellationToken cancel;
    std::string remotePath;

    bool isDone() const { return errored || bytesCompleted >= fileSize; }

    void markCancelled() {
        fail(SmbErrorCode::Cancelled, "Upload cancelled");
    }

    bool isStuck() const {
        if (errored || inFlight > 0) return false;
        if (nextOffset < static_cast<uint64_t>(fileSize)) return false;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (!slots[i].available) return false;
        }
        return bytesCompleted < fileSize;
    }

    // Range-contains lookup (tail re-issues use a mid-buffer pointer).
    int findSlot(const uint8_t* bufPtr) {
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].available) continue;
            const uint8_t* base = slots[i].buf.data();
            if (bufPtr >= base && bufPtr < base + slots[i].buf.size()) return i;
        }
        return -1;
    }

    void submitNextWrite() {
        if (errored || nextOffset >= static_cast<uint64_t>(fileSize)) return;
        if (cancel.cancelled()) {
            markCancelled();
            return;
        }

        int slot = -1;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].available) {
                slot = i;
                break;
            }
        }
        if (slot < 0) return;

        uint64_t remaining = static_cast<uint64_t>(fileSize) - nextOffset;
        uint32_t len = static_cast<uint32_t>(std::min(static_cast<uint64_t>(chunkSize), remaining));

        slots[slot].buf.resize(len);

        // Read chunk from local file
        if (fseeko(inFile, static_cast<off_t>(nextOffset), SEEK_SET) != 0 || fread(slots[slot].buf.data(), 1, len, inFile) != len) {
            fail(mapErrnoOrIo(errno), "Upload Failed: could not read from local file");
            return;
        }

        slots[slot].baseOffset = nextOffset;
        slots[slot].requested = len;
        slots[slot].written = 0;
        slots[slot].available = false;
        uint64_t offset = nextOffset;
        nextOffset += len;

        int ret = smb2_pwrite_async(ctx, fh, slots[slot].buf.data(), len, offset, writeCb, this);
        if (ret < 0) {
            const std::string msg = std::string("Upload Failed: smb2_pwrite_async: ") + smb2_get_error(ctx);
            fail(mapSmbResult(ret, msg), msg);
            slots[slot].available = true;
            nextOffset -= len;
            return;
        }
        inFlight++;
    }

    static void writeCb(smb2_context* smb2, int status, void* command_data, void* cb_data) {
        auto* st = static_cast<AsyncWriteState*>(cb_data);
        st->inFlight--;

        auto* cbd = static_cast<smb2_write_cb_data*>(command_data);
        int slotIdx = (cbd ? st->findSlot(cbd->buf) : -1);

        if (st->errored) {
            if (slotIdx >= 0) st->slots[slotIdx].available = true;
            return;
        }
        if (status < 0) {
            const std::string msg = std::string("Upload Failed: write error: ") + smb2_get_error(smb2);
            st->fail(mapSmbResult(status, msg), msg);
            if (slotIdx >= 0) st->slots[slotIdx].available = true;
            return;
        }
        if (slotIdx < 0) {
            st->fail(SmbErrorCode::Io, "Upload Failed: internal slot lookup failed");
            return;
        }

        auto& slot = st->slots[slotIdx];
        slot.written += static_cast<uint32_t>(status);
        st->bytesCompleted += status;

        // ── L1: short-write recovery ──
        if (slot.written < slot.requested) {
            if (status == 0) {
                st->fail(SmbErrorCode::Io, "Upload Failed: server accepted zero bytes");
                slot.available = true;
                return;
            }
            uint32_t tailLen = slot.requested - slot.written;
            uint64_t tailOffset = slot.baseOffset + slot.written;
            int ret = smb2_pwrite_async(st->ctx, st->fh, slot.buf.data() + slot.written, tailLen, tailOffset, writeCb, st);
            if (ret < 0) {
                const std::string msg = std::string("Upload Failed: short-write recovery smb2_pwrite_async: ") + smb2_get_error(st->ctx);
                st->fail(mapSmbResult(ret, msg), msg);
                slot.available = true;
                return;
            }
            st->inFlight++;
            return;
        }

        slot.available = true;

        if (st->progressHandler && st->bytesCompleted < st->fileSize && shouldEmitProgress(st->bytesCompleted, st->fileSize, st->lastEmittedBytes)) {
            st->progressHandler(static_cast<double>(st->bytesCompleted), static_cast<double>(st->fileSize));
        }

        st->submitNextWrite();
    }

    void cleanup() {
        if (fh && ctx) {
            // Best-effort on error/cancel path — finalizeRemoteFile() checks on success.
            smb2_close(ctx, fh);
            fh = nullptr;
        }
        if (inFile) {
            fclose(inFile);
            inFile = nullptr;
        }
    }

    bool finalizeRemoteFile() {
        if (fh && ctx) {
            const int syncResult = smb2_fsync(ctx, fh);
            if (syncResult < 0) {
                const std::string msg = std::string("Upload Failed: smb2_fsync: ") + smb2_get_error(ctx);
                fail(mapSmbResult(syncResult, msg), msg);
                smb2_close(ctx, fh);
                fh = nullptr;
                return false;
            }
            const int closeResult = smb2_close(ctx, fh);
            fh = nullptr;
            if (closeResult < 0) {
                const std::string msg = std::string("Upload Failed: smb2_close: ") + smb2_get_error(ctx);
                fail(mapSmbResult(closeResult, msg), msg);
                return false;
            }
            fh = nullptr;
        }
        if (inFile) {
            if (fclose(inFile) != 0) {
                const int err = errno;
                inFile = nullptr;
                fail(mapErrnoOrIo(err),
                     std::string("Upload Failed: fclose local input failed: ") + std::strerror(err));
                return false;
            }
            inFile = nullptr;
        }
        return true;
    }

    void cleanupOnError() {
        if (fh && ctx) {
            smb2_close(ctx, fh);
            fh = nullptr;
        }
        if (inFile) {
            fclose(inFile);
            inFile = nullptr;
        }
        if (ctx && !remotePath.empty()) {
            smb2_unlink(ctx, remotePath.c_str());
        }
    }
};

}  // anonymous namespace

// ── Public async entry points ────────────────────────────────────────────────
//
// Both functions expect to be called while the manager's connectionMutex_ is
// held (i.e. from inside a Handle::submitSync lambda). They drive their own
// poll()/smb2_service() loop on the calling thread.

namespace {

// No-progress idle deadline for transfer poll loops. Large files with steady
// progress are never timed out by this alone.
constexpr auto kTransferIdleTimeout = std::chrono::minutes(5);

enum class PollLoopResult {
    Done,
    TimedOut,
    PollFailed,
};

template <typename DoneFn, typename TickFn, typename ProgressFn>
PollLoopResult drivePollLoop(smb2_context* ctx, DoneFn&& done, TickFn&& tick, ProgressFn&& progressBytes) {
    using clock = std::chrono::steady_clock;
    auto lastProgressAt = clock::now();
    int64_t lastSeenBytes = progressBytes();

    struct pollfd pfd;
    while (!done()) {
        tick();
        if (done()) break;

        const int64_t nowBytes = progressBytes();
        if (nowBytes != lastSeenBytes) {
            lastSeenBytes = nowBytes;
            lastProgressAt = clock::now();
        } else if (clock::now() - lastProgressAt > kTransferIdleTimeout) {
            return PollLoopResult::TimedOut;
        }

        pfd.fd = smb2_get_fd(ctx);
        pfd.events = smb2_which_events(ctx);
        pfd.revents = 0;
        if (pfd.fd < 0 || pfd.events == 0) {
            // Nothing to wait on (e.g. all chunks already submitted and
            // libsmb2 has no pending work) — yield briefly.
            ::poll(nullptr, 0, 10);
            continue;
        }
        int ret = ::poll(&pfd, 1, 100 /*ms*/);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return PollLoopResult::PollFailed;
        }
        if (ret == 0) continue;  // timeout, retry
        int svc = smb2_service(ctx, pfd.revents);
        if (svc < 0) return PollLoopResult::PollFailed;
    }
    return PollLoopResult::Done;
}

[[noreturn]] void raiseTransferError(SmbErrorCode code, const std::string& message) {
    if (code != SmbErrorCode::Unknown) {
        SmbException::raise(code, message);
    }
    SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, message), message);
}

template <typename StateT>
void invalidateTransferContext(SmbConnectionManager& manager, StateT& state) {
    manager.invalidateContext();
    state.ctx = nullptr;
    if constexpr (requires { state.fh; }) {
        state.fh = nullptr;
    }
    if constexpr (requires { state.srcFh; }) {
        state.srcFh = nullptr;
    }
    if constexpr (requires { state.dstFh; }) {
        state.dstFh = nullptr;
    }
}

// Invalidate before unwinding state that outstanding callbacks may reference.
template <typename StateT>
void abortTransferTransport(PollLoopResult pollResult, SmbConnectionManager& manager, StateT& state) {
    const bool transportAbort = pollResult == PollLoopResult::TimedOut || pollResult == PollLoopResult::PollFailed;
    if (!transportAbort) return;
    if (!state.errored) {
        state.fail(SmbErrorCode::Io, "Transfer Failed: transport aborted");
    }
    invalidateTransferContext(manager, state);
}

}  // anonymous namespace

int64_t smbReadFileAsync(void* ctxVoid, SmbConnectionManager& manager, const std::string& remotePath, const std::string& localPath,
                         const std::string& taskId, std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    const std::string finalPath = localPath;
    const std::string tempPath = path_util::tempSiblingPath(finalPath, taskId.empty() ? "tmp" : taskId);

    AsyncReadState state;
    state.ctx = ctx;
    state.progressHandler = std::move(progressHandler);
    state.cancel = cancel;
    state.localPath = tempPath;

    // Open remote file
    state.fh = smb2_open(ctx, remotePath.c_str(), O_RDONLY);
    if (!state.fh) {
        const std::string msg = "Download Failed: Could not open remote file '" + remotePath + "': " + smb2_get_error(ctx);
        SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, msg), msg);
    }

    // Get file size
    struct smb2_stat_64 st;
    if (smb2_fstat(ctx, state.fh, &st) < 0) {
        std::string err = smb2_get_error(ctx);
        smb2_close(ctx, state.fh);
        SmbException::raise(SmbErrorCode::Io, "Download Failed: Could not stat '" + remotePath + "': " + err);
    }
    state.fileSize = static_cast<int64_t>(st.smb2_size);

    auto removeTempOnly = [&]() { std::remove(tempPath.c_str()); };

    // Handle empty file via temp → rename
    if (state.fileSize == 0) {
        if (cancel.cancelled()) {
            smb2_close(ctx, state.fh);
            state.fh = nullptr;
            SmbException::raise(SmbErrorCode::Cancelled, "Download cancelled");
        }
        FILE* f = fopen(tempPath.c_str(), "wb");
        if (!f) {
            smb2_close(ctx, state.fh);
            SmbException::raiseFromErrno(errno, "Download Failed: Could not create local temp file '" + tempPath + "'");
        }
        if (fclose(f) != 0) {
            const int err = errno;
            removeTempOnly();
            smb2_close(ctx, state.fh);
            SmbException::raiseFromErrno(err, "Download Failed: fclose empty temp failed");
        }
        if (smb2_close(ctx, state.fh) < 0) {
            removeTempOnly();
            SmbException::raise(SmbErrorCode::Io, std::string("Download Failed: smb2_close: ") + smb2_get_error(ctx));
        }
        state.fh = nullptr;
        if (cancel.cancelled()) {
            removeTempOnly();
            SmbException::raise(SmbErrorCode::Cancelled, "Download cancelled");
        }
        if (std::rename(tempPath.c_str(), finalPath.c_str()) != 0) {
            const int err = errno;
            removeTempOnly();
            SmbException::raiseFromErrno(err, "Download Failed: rename temp to final failed");
        }
        if (state.progressHandler) state.progressHandler(0.0, 0.0);
        return 0;
    }

    // Determine chunk size from server negotiation, capped for cancel responsiveness.
    state.chunkSize = capAsyncChunkSize(smb2_get_max_read_size(ctx));

    // Open local TEMP file for writing — never truncate finalPath until rename.
    std::remove(tempPath.c_str());  // clear stale leftover from a prior crash
    state.outFile = fopen(tempPath.c_str(), "wb");
    if (!state.outFile) {
        smb2_close(ctx, state.fh);
        state.fh = nullptr;
        SmbException::raiseFromErrno(errno, "Download Failed: Could not create local temp file '" + tempPath + "'");
    }

    if (state.progressHandler) state.progressHandler(0.0, static_cast<double>(state.fileSize));

    for (int i = 0; i < kMaxInFlight && !state.errored; i++) {
        state.submitNextRead();
    }

    const PollLoopResult pollResult = drivePollLoop(
        ctx, [&]() { return (state.isDone() && state.inFlight == 0) || state.isStuck(); },
        [&]() {
            if (cancel.cancelled() && !state.errored) {
                state.markCancelled();
            }
        },
        [&]() { return state.bytesCompleted; });

    if (state.isStuck()) {
        state.fail(SmbErrorCode::Io, "Download Failed: I/O pipeline stalled (unrecoverable short transfer)");
    }
    if (pollResult == PollLoopResult::TimedOut && !state.errored) {
        if (cancel.cancelled()) {
            state.markCancelled();
        } else {
            state.fail(SmbErrorCode::TimedOut, "Download Failed: transfer timed out (no progress)");
        }
    }
    if (pollResult == PollLoopResult::PollFailed && !state.errored) {
        state.fail(SmbErrorCode::Io, std::string("Download Failed: poll/service: ") + smb2_get_error(ctx));
    }

    abortTransferTransport(pollResult, manager, state);

    if (state.errored) {
        state.cleanup();
        removeTempOnly();
        raiseTransferError(state.errorCode, state.errorMsg);
    }

    if (!state.finalizeLocalFile()) {
        state.closeRemote();
        removeTempOnly();
        raiseTransferError(state.errorCode, state.errorMsg);
    }
    if (!state.closeRemote()) {
        removeTempOnly();
        SmbException::raise(SmbErrorCode::Io, state.errorMsg);
    }

    if (state.bytesCompleted != state.fileSize) {
        removeTempOnly();
        SmbException::raise(SmbErrorCode::Io, "Downloaded file size mismatch. Expected: " + std::to_string(state.fileSize) +
                                                  ", Got: " + std::to_string(state.bytesCompleted));
    }

    if (cancel.cancelled()) {
        removeTempOnly();
        SmbException::raise(SmbErrorCode::Cancelled, "Download cancelled");
    }
    if (std::rename(tempPath.c_str(), finalPath.c_str()) != 0) {
        const int err = errno;
        removeTempOnly();
        SmbException::raiseFromErrno(err, "Download Failed: rename temp to final failed");
    }

    if (state.progressHandler) state.progressHandler(static_cast<double>(state.fileSize), static_cast<double>(state.fileSize));
    return state.bytesCompleted;
}

namespace {

enum class RemotePathPresence {
    Exists,
    NotFound,
    Error,
};

RemotePathPresence remotePathPresence(smb2_context* ctx, const std::string& path, std::string* errorOut = nullptr, int* codeOut = nullptr) {
    struct smb2_stat_64 st {};
    const int r = smb2_stat(ctx, path.c_str(), &st);
    if (r == 0) return RemotePathPresence::Exists;
    const std::string err = smb2_get_error(ctx);
    const int mapped = static_cast<int>(mapSmbResult(r, err));
    if (mapped == static_cast<int>(SmbErrorCode::NotFound)) return RemotePathPresence::NotFound;
    if (errorOut) *errorOut = err;
    if (codeOut) *codeOut = mapped;
    return RemotePathPresence::Error;
}

bool remotePathExists(smb2_context* ctx, const std::string& path) {
    std::string err;
    int code = static_cast<int>(SmbErrorCode::Io);
    const auto presence = remotePathPresence(ctx, path, &err, &code);
    if (presence == RemotePathPresence::Exists) return true;
    if (presence == RemotePathPresence::NotFound) return false;
    SmbException::raise(code, "Upload Failed: could not stat '" + path + "': " + err);
}

// Commit remote temp → final. Samba/Windows often cannot rename over an existing
// name, so use a backup sibling and restore it if the final rename fails.
void commitRemoteRename(smb2_context* ctx, const std::string& tempPath, const std::string& finalPath, const std::string& taskId) {
    const bool finalExists = remotePathExists(ctx, finalPath);
    if (!finalExists) {
        const int r = smb2_rename(ctx, tempPath.c_str(), finalPath.c_str());
        if (r < 0) {
            SmbException::raiseFromErrnoResult(r, std::string("Upload Failed: smb2_rename temp→final: ") + smb2_get_error(ctx));
        }
        return;
    }

    const std::string backupPath = path_util::backupSiblingPath(finalPath, taskId.empty() ? "tmp" : taskId);
    if (remotePathExists(ctx, backupPath)) {
        SmbException::raise(SmbErrorCode::AlreadyExists,
                            "Upload Failed: backup path already exists from a prior attempt ('" + backupPath + "')");
    }

    const int moveAside = smb2_rename(ctx, finalPath.c_str(), backupPath.c_str());
    if (moveAside < 0) {
        SmbException::raiseFromErrnoResult(moveAside, std::string("Upload Failed: could not move existing destination aside: ") + smb2_get_error(ctx));
    }

    const int commit = smb2_rename(ctx, tempPath.c_str(), finalPath.c_str());
    if (commit < 0) {
        const std::string err = smb2_get_error(ctx);
        const int restore = smb2_rename(ctx, backupPath.c_str(), finalPath.c_str());
        if (restore < 0) {
            SmbException::raiseFromErrnoResult(
                commit, "Upload Failed: smb2_rename temp→final failed (" + err + ") and restore from backup also failed: " + smb2_get_error(ctx));
        }
        SmbException::raiseFromErrnoResult(commit, "Upload Failed: smb2_rename temp→final after backup: " + err);
    }

    if (smb2_unlink(ctx, backupPath.c_str()) < 0) {
        SMB_LOG_WARN("Upload backup cleanup failed");
    }
}

}  // namespace

int64_t smbWriteFileAsync(void* ctxVoid, SmbConnectionManager& manager, const std::string& localPath, const std::string& remotePath,
                          const std::string& taskId, std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    const std::string finalRemote = remotePath;
    const std::string tempRemote = path_util::tempSiblingPath(finalRemote, taskId.empty() ? "tmp" : taskId);

    AsyncWriteState state;
    state.ctx = ctx;
    state.progressHandler = std::move(progressHandler);
    state.cancel = cancel;
    state.remotePath = tempRemote;  // cleanupOnError unlinks temp only

    state.inFile = fopen(localPath.c_str(), "rb");
    if (!state.inFile) {
        SmbException::raiseFromErrno(errno, "Upload Failed: Could not open local file '" + localPath + "'");
    }
    fseeko(state.inFile, 0, SEEK_END);
    int64_t localFileSize = static_cast<int64_t>(ftello(state.inFile));
    fseeko(state.inFile, 0, SEEK_SET);
    if (localFileSize < 0) {
        fclose(state.inFile);
        state.inFile = nullptr;
        SmbException::raise(SmbErrorCode::Io, "Upload Failed: Could not determine size of '" + localPath + "'");
    }
    state.fileSize = localFileSize;

    auto unlinkTempOnly = [&]() {
        if (ctx) smb2_unlink(ctx, tempRemote.c_str());
    };

    // Empty file: create temp then rename to final.
    if (state.fileSize == 0) {
        if (cancel.cancelled()) {
            fclose(state.inFile);
            state.inFile = nullptr;
            SmbException::raise(SmbErrorCode::Cancelled, "Upload cancelled");
        }
        smb2_unlink(ctx, tempRemote.c_str());
        smb2fh* fh = smb2_open(ctx, tempRemote.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        if (!fh) {
            fclose(state.inFile);
            state.inFile = nullptr;
            const std::string msg = "Upload Failed: Could not create remote temp '" + tempRemote + "': " + smb2_get_error(ctx);
            SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, msg), msg);
        }
        if (smb2_fsync(ctx, fh) < 0) {
            const std::string err = smb2_get_error(ctx);
            smb2_close(ctx, fh);
            unlinkTempOnly();
            fclose(state.inFile);
            SmbException::raise(SmbErrorCode::Io, "Upload Failed: smb2_fsync empty temp: " + err);
        }
        if (smb2_close(ctx, fh) < 0) {
            const std::string err = smb2_get_error(ctx);
            unlinkTempOnly();
            fclose(state.inFile);
            SmbException::raise(SmbErrorCode::Io, "Upload Failed: smb2_close empty temp: " + err);
        }
        fclose(state.inFile);
        state.inFile = nullptr;
        if (cancel.cancelled()) {
            unlinkTempOnly();
            SmbException::raise(SmbErrorCode::Cancelled, "Upload cancelled");
        }
        try {
            commitRemoteRename(ctx, tempRemote, finalRemote, taskId);
        } catch (...) {
            unlinkTempOnly();
            throw;
        }
        if (state.progressHandler) state.progressHandler(0.0, 0.0);
        return 0;
    }

    smb2_unlink(ctx, tempRemote.c_str());
    state.fh = smb2_open(ctx, tempRemote.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    if (!state.fh) {
        std::string err = smb2_get_error(ctx);
        fclose(state.inFile);
        state.inFile = nullptr;
        const std::string msg = "Upload Failed: Could not create remote temp '" + tempRemote + "': " + err;
        SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, msg), msg);
    }

    state.chunkSize = capAsyncChunkSize(smb2_get_max_write_size(ctx));

    if (state.progressHandler) state.progressHandler(0.0, static_cast<double>(state.fileSize));

    for (int i = 0; i < kMaxInFlight && !state.errored; i++) {
        state.submitNextWrite();
    }

    const PollLoopResult pollResult = drivePollLoop(
        ctx, [&]() { return (state.isDone() && state.inFlight == 0) || state.isStuck(); },
        [&]() {
            if (cancel.cancelled() && !state.errored) {
                state.markCancelled();
            }
        },
        [&]() { return state.bytesCompleted; });

    if (state.isStuck()) {
        state.fail(SmbErrorCode::Io, "Upload Failed: I/O pipeline stalled (unrecoverable short transfer)");
    }
    if (pollResult == PollLoopResult::TimedOut && !state.errored) {
        if (cancel.cancelled()) {
            state.markCancelled();
        } else {
            state.fail(SmbErrorCode::TimedOut, "Upload Failed: transfer timed out (no progress)");
        }
    }
    if (pollResult == PollLoopResult::PollFailed && !state.errored) {
        state.fail(SmbErrorCode::Io, std::string("Upload Failed: poll/service: ") + smb2_get_error(ctx));
    }

    abortTransferTransport(pollResult, manager, state);

    if (state.errored) {
        state.cleanupOnError();
        raiseTransferError(state.errorCode, state.errorMsg);
    }

    if (!state.finalizeRemoteFile()) {
        unlinkTempOnly();
        raiseTransferError(state.errorCode, state.errorMsg);
    }

    if (state.bytesCompleted != state.fileSize) {
        unlinkTempOnly();
        SmbException::raise(SmbErrorCode::Io, "Uploaded file size mismatch. Expected: " + std::to_string(state.fileSize) +
                                                  ", Uploaded: " + std::to_string(state.bytesCompleted));
    }

    if (cancel.cancelled()) {
        unlinkTempOnly();
        SmbException::raise(SmbErrorCode::Cancelled, "Upload cancelled");
    }
    try {
        commitRemoteRename(ctx, tempRemote, finalRemote, taskId);
    } catch (...) {
        unlinkTempOnly();
        throw;
    }

    if (state.progressHandler) state.progressHandler(static_cast<double>(state.fileSize), static_cast<double>(state.fileSize));
    return state.bytesCompleted;
}

// ─── Async pipelined SMB→SMB copy ────────────────────────────────────────────
//
// One smb2_context, two open file handles (src + dst). Up to kMaxInFlight
// chunks are bouncing between Reading and Writing on the wire at any time;
// when a read completes the same buffer is immediately submitted as a write,
// freeing read bandwidth for the next chunk only when its write completes.
// Drives one poll/service loop on the calling thread under connectionMutex_.

namespace {

struct AsyncCopyState {
    smb2_context* ctx = nullptr;
    smb2fh* srcFh = nullptr;
    smb2fh* dstFh = nullptr;

    int64_t fileSize = 0;
    uint32_t chunkSize = 0;

    uint64_t nextReadOffset = 0;
    int64_t bytesCopied = 0;  // bytes that finished writing (for progress)
    int64_t lastEmittedBytes = 0;
    int inFlight = 0;
    bool errored = false;
    SmbErrorCode errorCode{SmbErrorCode::Unknown};
    std::string errorMsg;

    void fail(SmbErrorCode code, std::string msg) {
        if (errored) return;
        errored = true;
        errorCode = code;
        errorMsg = std::move(msg);
    }

    struct ChunkSlot {
        enum State { Idle, Reading, Writing };
        State state = Idle;
        std::vector<uint8_t> buf;
        uint64_t baseOffset = 0;  // file offset corresponding to buf[0]
        uint32_t requested = 0;   // total chunk size to copy (== buf.size())
        uint32_t read = 0;        // src bytes received so far (≤ requested)
        uint32_t written = 0;     // dst bytes acknowledged so far (≤ read)
    };
    ChunkSlot slots[kMaxInFlight];

    struct CbData {
        AsyncCopyState* st;
        int slotIdx;
    };
    CbData cbDatas[kMaxInFlight];

    // Shared cumulative counter for multi-file (recursive) copies.
    std::shared_ptr<int64_t> totalBytesCopied;
    int64_t totalSize = 0;
    std::function<void(double, double)> progressHandler;
    CancellationToken cancel;
    std::string toPath;

    void init() {
        for (int i = 0; i < kMaxInFlight; i++) {
            cbDatas[i] = {this, i};
        }
    }

    bool isDone() const {
        if (errored) return true;
        if (nextReadOffset < static_cast<uint64_t>(fileSize)) return false;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].state != ChunkSlot::Idle) return false;
        }
        return true;
    }

    // Safety net: ranges fully requested, no in-flight work, all slots
    // idle, but not all bytes copied. Prevents the poll loop from
    // exiting cleanly with a truncated destination.
    bool isStuck() const {
        if (errored || inFlight > 0) return false;
        if (nextReadOffset < static_cast<uint64_t>(fileSize)) return false;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].state != ChunkSlot::Idle) return false;
        }
        return bytesCopied < fileSize;
    }

    void submitNextRead() {
        if (errored || nextReadOffset >= static_cast<uint64_t>(fileSize)) return;
        if (cancel.cancelled()) {
            fail(SmbErrorCode::Cancelled, "Copy cancelled");
            return;
        }

        int slot = -1;
        for (int i = 0; i < kMaxInFlight; i++) {
            if (slots[i].state == ChunkSlot::Idle) {
                slot = i;
                break;
            }
        }
        if (slot < 0) return;

        uint64_t remaining = static_cast<uint64_t>(fileSize) - nextReadOffset;
        uint32_t len = static_cast<uint32_t>(std::min(static_cast<uint64_t>(chunkSize), remaining));

        slots[slot].buf.resize(len);
        slots[slot].baseOffset = nextReadOffset;
        slots[slot].requested = len;
        slots[slot].read = 0;
        slots[slot].written = 0;
        slots[slot].state = ChunkSlot::Reading;
        nextReadOffset += len;

        int ret = smb2_pread_async(ctx, srcFh, slots[slot].buf.data(), len, slots[slot].baseOffset, readCb, &cbDatas[slot]);
        if (ret < 0) {
            const std::string msg = std::string("Copy Failed: smb2_pread_async: ") + smb2_get_error(ctx);
            fail(mapSmbResult(ret, msg), msg);
            slots[slot].state = ChunkSlot::Idle;
            nextReadOffset -= len;
            return;
        }
        inFlight++;
    }

    // Issue (or re-issue) a write for the un-written tail of this slot.
    void submitWriteForSlot(int slotIdx) {
        if (errored) {
            slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }
        auto& slot = slots[slotIdx];
        slot.state = ChunkSlot::Writing;
        uint32_t tailLen = slot.read - slot.written;
        uint64_t tailOffset = slot.baseOffset + slot.written;
        int ret = smb2_pwrite_async(ctx, dstFh, slot.buf.data() + slot.written, tailLen, tailOffset, writeCb, &cbDatas[slotIdx]);
        if (ret < 0) {
            const std::string msg = std::string("Copy Failed: smb2_pwrite_async: ") + smb2_get_error(ctx);
            fail(mapSmbResult(ret, msg), msg);
            slot.state = ChunkSlot::Idle;
            return;
        }
        inFlight++;
    }

    static void readCb(smb2_context* smb2, int status, void* /*command_data*/, void* cb_data) {
        auto* d = static_cast<CbData*>(cb_data);
        auto* st = d->st;
        int slotIdx = d->slotIdx;
        st->inFlight--;

        if (st->errored) {
            st->slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }
        if (status < 0) {
            const std::string msg = std::string("Copy Failed: read error: ") + smb2_get_error(smb2);
            st->fail(mapSmbResult(status, msg), msg);
            st->slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }

        auto& slot = st->slots[slotIdx];
        slot.read += static_cast<uint32_t>(status);

        // ── L1: short-read recovery (re-issue tail on same slot) ──
        if (slot.read < slot.requested) {
            if (status == 0) {
                st->fail(SmbErrorCode::Io, "Copy Failed: unexpected EOF on source (zero-byte read)");
                slot.state = ChunkSlot::Idle;
                return;
            }
            uint32_t tailLen = slot.requested - slot.read;
            uint64_t tailOffset = slot.baseOffset + slot.read;
            int ret = smb2_pread_async(st->ctx, st->srcFh, slot.buf.data() + slot.read, tailLen, tailOffset, readCb, &st->cbDatas[slotIdx]);
            if (ret < 0) {
                const std::string msg = std::string("Copy Failed: short-read recovery smb2_pread_async: ") + smb2_get_error(st->ctx);
                st->fail(mapSmbResult(ret, msg), msg);
                slot.state = ChunkSlot::Idle;
                return;
            }
            st->inFlight++;
            return;
        }

        // Slot fully read → start the write phase.
        st->submitWriteForSlot(slotIdx);
    }

    static void writeCb(smb2_context* smb2, int status, void* /*command_data*/, void* cb_data) {
        auto* d = static_cast<CbData*>(cb_data);
        auto* st = d->st;
        int slotIdx = d->slotIdx;
        st->inFlight--;

        if (st->errored) {
            st->slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }
        if (status < 0) {
            const std::string msg = std::string("Copy Failed: write error: ") + smb2_get_error(smb2);
            st->fail(mapSmbResult(status, msg), msg);
            st->slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }

        auto& slot = st->slots[slotIdx];
        slot.written += static_cast<uint32_t>(status);

        // ── L1: short-write recovery (re-issue tail on same slot) ──
        if (slot.written < slot.read) {
            if (status == 0) {
                st->fail(SmbErrorCode::Io, "Copy Failed: destination accepted zero bytes");
                slot.state = ChunkSlot::Idle;
                return;
            }
            st->submitWriteForSlot(slotIdx);
            return;
        }

        // Slot fully written → free it and credit progress.
        uint32_t copied = slot.requested;
        slot.state = ChunkSlot::Idle;
        st->bytesCopied += copied;
        if (st->totalBytesCopied) *st->totalBytesCopied += copied;

        if (st->progressHandler && st->totalSize > 0) {
            int64_t cur = st->totalBytesCopied ? *st->totalBytesCopied : st->bytesCopied;
            if (cur < st->totalSize && shouldEmitProgress(cur, st->totalSize, st->lastEmittedBytes)) {
                st->progressHandler(static_cast<double>(cur), static_cast<double>(st->totalSize));
            }
        }

        // Slot is free; submit next read to keep the pipeline saturated.
        st->submitNextRead();
    }

    void cleanup() {
        if (srcFh && ctx) {
            smb2_close(ctx, srcFh);
            srcFh = nullptr;
        }
        if (dstFh && ctx) {
            const int syncResult = smb2_fsync(ctx, dstFh);
            const int closeResult = smb2_close(ctx, dstFh);
            dstFh = nullptr;
            if (syncResult < 0) {
                const std::string msg = std::string("Copy Failed: smb2_fsync: ") + smb2_get_error(ctx);
                fail(mapSmbResult(syncResult, msg), msg);
            } else if (closeResult < 0) {
                const std::string msg = std::string("Copy Failed: smb2_close: ") + smb2_get_error(ctx);
                fail(mapSmbResult(closeResult, msg), msg);
            }
        }
    }

    void cleanupOnError() {
        if (srcFh && ctx) {
            smb2_close(ctx, srcFh);
            srcFh = nullptr;
        }
        if (dstFh && ctx) {
            smb2_close(ctx, dstFh);
            dstFh = nullptr;
        }
        if (ctx && !toPath.empty()) {
            smb2_unlink(ctx, toPath.c_str());
        }
    }
};

}  // anonymous namespace

int64_t smbCopyFileAsync(void* ctxVoid, SmbConnectionManager& manager, const std::string& fromPath, const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                         std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    AsyncCopyState state;
    state.ctx = ctx;
    state.totalBytesCopied = totalBytesCopied;
    state.totalSize = totalSize;
    state.progressHandler = std::move(progressHandler);
    state.cancel = cancel;
    state.toPath = toPath;
    state.init();

    // Open source.
    state.srcFh = smb2_open(ctx, fromPath.c_str(), O_RDONLY);
    if (!state.srcFh) {
        const std::string msg = "Copy Failed: Could not open source '" + fromPath + "': " + smb2_get_error(ctx);
        SmbException::raise(mapSmbResult(0, msg), msg);
    }

    // Size source.
    struct smb2_stat_64 st;
    const int statResult = smb2_fstat(ctx, state.srcFh, &st);
    if (statResult < 0) {
        const std::string msg = "Copy Failed: Could not stat source '" + fromPath + "': " + smb2_get_error(ctx);
        smb2_close(ctx, state.srcFh);
        SmbException::raise(mapSmbResult(statResult, msg), msg);
    }
    state.fileSize = static_cast<int64_t>(st.smb2_size);

    // Open destination (exclusive create).
    state.dstFh = smb2_open(ctx, toPath.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (!state.dstFh) {
        const std::string msg = "Copy Failed: Could not create destination '" + toPath + "': " + smb2_get_error(ctx);
        smb2_close(ctx, state.srcFh);
        state.srcFh = nullptr;
        SmbException::raise(mapSmbResult(0, msg), msg);
    }

    // Handle empty file: nothing to read/write — still fsync/close via cleanup().
    if (state.fileSize == 0) {
        if (cancel.cancelled()) {
            state.cleanupOnError();
            SmbException::raise(SmbErrorCode::Cancelled, "Copy cancelled");
        }
        state.cleanup();
        if (state.errored) {
            state.cleanupOnError();
            raiseTransferError(state.errorCode, state.errorMsg);
        }
        return 0;
    }

    state.chunkSize = capAsyncChunkSize(smb2_get_max_read_size(ctx));
    uint32_t maxW = capAsyncChunkSize(smb2_get_max_write_size(ctx));
    if (maxW < state.chunkSize) state.chunkSize = maxW;

    // Kick off first batch of reads.
    for (int i = 0; i < kMaxInFlight && !state.errored; i++) {
        state.submitNextRead();
    }

    const PollLoopResult pollResult = drivePollLoop(
        ctx, [&]() { return (state.isDone() && state.inFlight == 0) || state.isStuck(); },
        [&]() {
            if (cancel.cancelled() && !state.errored) {
                state.fail(SmbErrorCode::Cancelled, "Copy cancelled");
            }
        },
        [&]() { return state.bytesCopied; });
    if (state.isStuck()) {
        state.fail(SmbErrorCode::Io, "Copy Failed: I/O pipeline stalled (unrecoverable short transfer)");
    }
    if (pollResult == PollLoopResult::TimedOut && !state.errored) {
        if (cancel.cancelled()) {
            state.fail(SmbErrorCode::Cancelled, "Copy cancelled");
        } else {
            state.fail(SmbErrorCode::TimedOut, "Copy Failed: transfer timed out (no progress)");
        }
    }
    if (pollResult == PollLoopResult::PollFailed && !state.errored) {
        state.fail(SmbErrorCode::Io, std::string("Copy Failed: poll/service: ") + smb2_get_error(ctx));
    }

    abortTransferTransport(pollResult, manager, state);

    if (state.errored) {
        state.cleanupOnError();
        raiseTransferError(state.errorCode, state.errorMsg);
    }

    if (state.bytesCopied != state.fileSize) {
        state.cleanupOnError();
        SmbException::raise(SmbErrorCode::Io, "Copied file size mismatch. Expected: " + std::to_string(state.fileSize) +
                                                  ", Got: " + std::to_string(state.bytesCopied));
    }

    state.cleanup();
    if (state.errored) {
        raiseTransferError(state.errorCode, state.errorMsg);
    }
    return state.bytesCopied;
}

}  // namespace react_native_smb
