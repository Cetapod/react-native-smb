#include "../SmbFileIO.hpp"
#include "SmbFileCopyInternal.hpp"
#include "SmbTransferCore.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <smb2/libsmb2.h>

namespace react_native_smb {
namespace {

using detail::TransferCore;

struct CopyState {
    enum class Phase { Idle, Reading, Writing };

    struct Slot {
        Phase phase = Phase::Idle;
        std::vector<uint8_t> buffer;
        uint64_t offset = 0;
        uint32_t requested = 0;
        uint32_t read = 0;
        uint32_t written = 0;
    };

    struct CallbackData {
        CopyState* state;
        int slot;
    };

    TransferCore core;
    smb2fh* source = nullptr;
    smb2fh* destination = nullptr;
    std::string destinationPath;
    Slot slots[detail::kMaxInFlight];
    CallbackData callbacks[detail::kMaxInFlight];
    std::shared_ptr<int64_t> totalCopied;
    int64_t totalSize = 0;

    void init() {
        for (int i = 0; i < detail::kMaxInFlight; ++i) {
            callbacks[i] = {this, i};
        }
    }

    void fail(SmbErrorCode code, std::string message) { core.failure.fail(code, std::move(message)); }

    void cancel() { fail(SmbErrorCode::Cancelled, "Copy cancelled"); }

    bool done() const {
        if (core.failure.errored) return true;
        if (core.nextOffset < static_cast<uint64_t>(core.fileSize)) return false;
        for (const auto& slot : slots) {
            if (slot.phase != Phase::Idle) return false;
        }
        return true;
    }

    bool stuck() const {
        return !core.failure.errored && core.inFlight == 0 && done() && core.completedBytes < core.fileSize;
    }

    void submitRead() {
        if (core.failure.errored || core.nextOffset >= static_cast<uint64_t>(core.fileSize)) return;
        if (core.cancel.cancelled()) {
            cancel();
            return;
        }

        int index = -1;
        for (int i = 0; i < detail::kMaxInFlight; ++i) {
            if (slots[i].phase == Phase::Idle) {
                index = i;
                break;
            }
        }
        if (index < 0) return;

        auto& slot = slots[index];
        const uint32_t length = static_cast<uint32_t>(
            std::min<uint64_t>(core.chunkSize, static_cast<uint64_t>(core.fileSize) - core.nextOffset));
        slot.buffer.resize(length);
        slot.offset = core.nextOffset;
        slot.requested = length;
        slot.read = 0;
        slot.written = 0;
        slot.phase = Phase::Reading;
        core.nextOffset += length;

        const int result = smb2_pread_async(core.ctx, source, slot.buffer.data(), length, slot.offset, readCallback,
                                            &callbacks[index]);
        if (result < 0) {
            const std::string message = std::string("Copy Failed: smb2_pread_async: ") + smb2_get_error(core.ctx);
            fail(detail::mapSmbResult(result, message), message);
            slot.phase = Phase::Idle;
            core.nextOffset -= length;
            return;
        }
        ++core.inFlight;
    }

    void submitWrite(int index) {
        auto& slot = slots[index];
        if (core.failure.errored) {
            slot.phase = Phase::Idle;
            return;
        }

        slot.phase = Phase::Writing;
        const uint32_t tail = slot.read - slot.written;
        const int result = smb2_pwrite_async(core.ctx, destination, slot.buffer.data() + slot.written, tail,
                                             slot.offset + slot.written, writeCallback, &callbacks[index]);
        if (result < 0) {
            const std::string message = std::string("Copy Failed: smb2_pwrite_async: ") + smb2_get_error(core.ctx);
            fail(detail::mapSmbResult(result, message), message);
            slot.phase = Phase::Idle;
            return;
        }
        ++core.inFlight;
    }

    static void readCallback(smb2_context* ctx, int status, void*, void* callbackData) {
        auto* data = static_cast<CallbackData*>(callbackData);
        auto& state = *data->state;
        auto& slot = state.slots[data->slot];
        --state.core.inFlight;

        if (state.core.failure.errored) {
            slot.phase = Phase::Idle;
            return;
        }
        if (status < 0) {
            const std::string message = std::string("Copy Failed: read error: ") + smb2_get_error(ctx);
            state.fail(detail::mapSmbResult(status, message), message);
            slot.phase = Phase::Idle;
            return;
        }
        if (static_cast<uint64_t>(status) > slot.requested - slot.read) {
            state.fail(SmbErrorCode::Io, "Copy Failed: invalid read completion size");
            slot.phase = Phase::Idle;
            return;
        }

        slot.read += static_cast<uint32_t>(status);
        if (slot.read < slot.requested) {
            if (status == 0) {
                state.fail(SmbErrorCode::Io, "Copy Failed: unexpected EOF on source (zero-byte read)");
                slot.phase = Phase::Idle;
                return;
            }
            const uint32_t tail = slot.requested - slot.read;
            const int result = smb2_pread_async(state.core.ctx, state.source, slot.buffer.data() + slot.read, tail,
                                                slot.offset + slot.read, readCallback, callbackData);
            if (result < 0) {
                const std::string message =
                    std::string("Copy Failed: short-read recovery smb2_pread_async: ") + smb2_get_error(state.core.ctx);
                state.fail(detail::mapSmbResult(result, message), message);
                slot.phase = Phase::Idle;
                return;
            }
            ++state.core.inFlight;
            return;
        }

        state.submitWrite(data->slot);
    }

    static void writeCallback(smb2_context* ctx, int status, void*, void* callbackData) {
        auto* data = static_cast<CallbackData*>(callbackData);
        auto& state = *data->state;
        auto& slot = state.slots[data->slot];
        --state.core.inFlight;

        if (state.core.failure.errored) {
            slot.phase = Phase::Idle;
            return;
        }
        if (status < 0) {
            const std::string message = std::string("Copy Failed: write error: ") + smb2_get_error(ctx);
            state.fail(detail::mapSmbResult(status, message), message);
            slot.phase = Phase::Idle;
            return;
        }
        if (static_cast<uint64_t>(status) > slot.read - slot.written) {
            state.fail(SmbErrorCode::Io, "Copy Failed: invalid write completion size");
            slot.phase = Phase::Idle;
            return;
        }

        slot.written += static_cast<uint32_t>(status);
        if (slot.written < slot.read) {
            if (status == 0) {
                state.fail(SmbErrorCode::Io, "Copy Failed: destination accepted zero bytes");
                slot.phase = Phase::Idle;
                return;
            }
            state.submitWrite(data->slot);
            return;
        }

        const uint32_t copied = slot.requested;
        slot.phase = Phase::Idle;
        state.core.completedBytes += copied;
        if (state.totalCopied) *state.totalCopied += copied;
        state.core.emitProgress(state.totalCopied ? *state.totalCopied : state.core.completedBytes, state.totalSize);
        state.submitRead();
    }

    void cleanup() {
        if (source && core.ctx) {
            smb2_close(core.ctx, source);
            source = nullptr;
        }
        if (destination && core.ctx) {
            const int sync = smb2_fsync(core.ctx, destination);
            const int close = smb2_close(core.ctx, destination);
            destination = nullptr;
            if (sync < 0) {
                const std::string message = std::string("Copy Failed: smb2_fsync: ") + smb2_get_error(core.ctx);
                fail(detail::mapSmbResult(sync, message), message);
            } else if (close < 0) {
                const std::string message = std::string("Copy Failed: smb2_close: ") + smb2_get_error(core.ctx);
                fail(detail::mapSmbResult(close, message), message);
            }
        }
    }

    void cleanupOnError() {
        if (source && core.ctx) {
            smb2_close(core.ctx, source);
            source = nullptr;
        }
        if (destination && core.ctx) {
            smb2_close(core.ctx, destination);
            destination = nullptr;
        }
        if (core.ctx) smb2_unlink(core.ctx, destinationPath.c_str());
    }
};

}  // namespace

int64_t smbCopyFileAsync(void* context, SmbConnectionManager& manager, const std::string& fromPath,
                         const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                         std::function<void(double, double)> progressHandler, const CancellationToken& cancel,
                         std::function<void()> onDestinationClaimed) {
    auto* ctx = static_cast<smb2_context*>(context);
    CopyState state;
    state.init();
    state.core.ctx = ctx;
    state.core.cancel = cancel;
    state.core.progressHandler = std::move(progressHandler);
    state.totalCopied = std::move(totalBytesCopied);
    state.totalSize = totalSize;
    state.destinationPath = toPath;

    state.source = smb2_open(ctx, fromPath.c_str(), O_RDONLY);
    if (!state.source) {
        const std::string message = "Copy Failed: Could not open source '" + fromPath + "': " + smb2_get_error(ctx);
        SmbException::raise(detail::mapSmbResult(0, message), message);
    }
    smb2_stat_64 stat{};
    const int statResult = smb2_fstat(ctx, state.source, &stat);
    if (statResult < 0) {
        const std::string message = "Copy Failed: Could not stat source '" + fromPath + "': " + smb2_get_error(ctx);
        smb2_close(ctx, state.source);
        SmbException::raise(detail::mapSmbResult(statResult, message), message);
    }

    state.core.fileSize = static_cast<int64_t>(stat.smb2_size);
    state.destination = smb2_open(ctx, toPath.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (!state.destination) {
        const std::string message =
            "Copy Failed: Could not create destination '" + toPath + "': " + smb2_get_error(ctx);
        smb2_close(ctx, state.source);
        SmbException::raise(detail::mapSmbResult(0, message), message);
    }
    if (onDestinationClaimed) {
        try {
            onDestinationClaimed();
        } catch (...) {
            state.cleanupOnError();
            throw;
        }
    }
    if (state.core.cancel.cancelled()) {
        state.cleanupOnError();
        SmbException::raise(SmbErrorCode::Cancelled, "Copy cancelled");
    }
    if (state.core.fileSize == 0) {
        state.cleanup();
        if (state.core.failure.errored) {
            state.cleanupOnError();
            detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
        }
        return 0;
    }

    state.core.chunkSize = std::min(detail::capAsyncChunkSize(smb2_get_max_read_size(ctx)),
                                    detail::capAsyncChunkSize(smb2_get_max_write_size(ctx)));
    detail::runTransfer(
        state.core, manager,
        [&] {
            for (int i = 0; i < detail::kMaxInFlight && !state.core.failure.errored; ++i) state.submitRead();
        },
        [&] { return state.done() && state.core.inFlight == 0; }, [&] { return state.stuck(); },
        [&] { state.cancel(); },
        [&] {
            state.source = nullptr;
            state.destination = nullptr;
        },
        "Copy Failed");
    if (state.core.failure.errored) {
        state.cleanupOnError();
        detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
    }
    if (state.core.completedBytes != state.core.fileSize) {
        state.cleanupOnError();
        SmbException::raise(SmbErrorCode::Io,
                            "Copied file size mismatch. Expected: " + std::to_string(state.core.fileSize) +
                                ", Got: " + std::to_string(state.core.completedBytes));
    }

    state.cleanup();
    if (state.core.failure.errored) detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
    return state.core.completedBytes;
}

int64_t smbCopyFileAsync(void* context, SmbConnectionManager& manager, const std::string& fromPath,
                         const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                         std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    return smbCopyFileAsync(context, manager, fromPath, toPath, std::move(totalBytesCopied), totalSize,
                            std::move(progressHandler), cancel, {});
}

}  // namespace react_native_smb
