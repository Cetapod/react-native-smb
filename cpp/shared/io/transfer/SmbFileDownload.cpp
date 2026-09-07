#include "../SmbFileIO.hpp"
#include "SmbTransferCore.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <smb2/libsmb2.h>

#include "../../util/SmbErrorMapper.hpp"
#include "../SmbPathUtil.hpp"

namespace react_native_smb {
namespace {

using detail::TransferCore;

struct DownloadState {
    struct Slot {
        std::vector<uint8_t> buffer;
        uint64_t offset = 0;
        uint32_t requested = 0;
        uint32_t received = 0;
        bool available = true;
    };

    struct CallbackData {
        DownloadState* state;
        int slot;
    };

    TransferCore core;
    smb2fh* fh = nullptr;
    FILE* outFile = nullptr;
    Slot slots[detail::kMaxInFlight];
    CallbackData callbacks[detail::kMaxInFlight];

    void init() {
        for (int i = 0; i < detail::kMaxInFlight; ++i) {
            callbacks[i] = {this, i};
        }
    }

    void fail(SmbErrorCode code, std::string message) { core.failure.fail(code, std::move(message)); }

    void cancel() { fail(SmbErrorCode::Cancelled, "Download cancelled"); }

    bool done() const { return core.failure.errored || core.completedBytes >= core.fileSize; }

    bool stuck() const {
        if (core.failure.errored || core.inFlight || core.nextOffset < static_cast<uint64_t>(core.fileSize))
            return false;
        for (const auto& slot : slots) {
            if (!slot.available) return false;
        }
        return core.completedBytes < core.fileSize;
    }

    void submitNext() {
        if (core.failure.errored || core.nextOffset >= static_cast<uint64_t>(core.fileSize)) return;
        if (core.cancel.cancelled()) return cancel();

        int index = -1;
        for (int i = 0; i < detail::kMaxInFlight; ++i) {
            if (slots[i].available) {
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
        slot.received = 0;
        slot.available = false;
        core.nextOffset += length;

        const int result =
            smb2_pread_async(core.ctx, fh, slot.buffer.data(), length, slot.offset, readCallback, &callbacks[index]);
        if (result < 0) {
            const std::string message = std::string("Download Failed: smb2_pread_async: ") + smb2_get_error(core.ctx);
            fail(detail::mapSmbResult(result, message), message);
            slot.available = true;
            core.nextOffset -= length;
            return;
        }
        ++core.inFlight;
    }

    static void readCallback(smb2_context* ctx, int status, void* commandData, void* callbackData) {
        auto* data = static_cast<CallbackData*>(callbackData);
        auto& state = *data->state;
        auto& slot = state.slots[data->slot];
        --state.core.inFlight;

        if (state.core.failure.errored) {
            slot.available = true;
            return;
        }
        if (status < 0) {
            const std::string message = std::string("Download Failed: read error: ") + smb2_get_error(ctx);
            state.fail(detail::mapSmbResult(status, message), message);
            slot.available = true;
            return;
        }

        const uint32_t remaining = slot.requested - slot.received;
        if (static_cast<uint64_t>(status) > remaining || !commandData) {
            state.fail(SmbErrorCode::Io, "Download Failed: invalid read completion size");
            slot.available = true;
            return;
        }

        auto* response = static_cast<smb2_read_cb_data*>(commandData);
        const uint64_t writeOffset = slot.offset + slot.received;
        if (status > 0 &&
            (!state.outFile || fseeko(state.outFile, static_cast<off_t>(writeOffset), SEEK_SET) != 0 ||
             fwrite(response->buf, 1, static_cast<size_t>(status), state.outFile) != static_cast<size_t>(status))) {
            state.fail(detail::mapErrnoOrIo(errno), "Download Failed: local file write error");
            slot.available = true;
            return;
        }

        slot.received += static_cast<uint32_t>(status);
        state.core.completedBytes += status;
        if (slot.received < slot.requested) {
            if (status == 0) {
                state.fail(SmbErrorCode::Io, "Download Failed: unexpected EOF (zero-byte read)");
                slot.available = true;
                return;
            }
            const uint32_t tail = slot.requested - slot.received;
            const int result = smb2_pread_async(state.core.ctx, state.fh, slot.buffer.data() + slot.received, tail,
                                                slot.offset + slot.received, readCallback, callbackData);
            if (result < 0) {
                const std::string message = std::string("Download Failed: short-read recovery smb2_pread_async: ") +
                                            smb2_get_error(state.core.ctx);
                state.fail(detail::mapSmbResult(result, message), message);
                slot.available = true;
                return;
            }
            ++state.core.inFlight;
            return;
        }

        slot.available = true;
        state.core.emitProgress(state.core.completedBytes, state.core.fileSize);
        state.submitNext();
    }

    void cleanup() {
        if (outFile) {
            fclose(outFile);
            outFile = nullptr;
        }
        if (fh && core.ctx) {
            smb2_close(core.ctx, fh);
            fh = nullptr;
        }
    }

    bool finalizeLocal() {
        if (!outFile) return true;
        if (fflush(outFile) != 0) {
            const int error = errno;
            fclose(outFile);
            outFile = nullptr;
            fail(detail::mapErrnoOrIo(error), std::string("Download Failed: fflush failed: ") + std::strerror(error));
            return false;
        }
        if (fclose(outFile) != 0) {
            const int error = errno;
            outFile = nullptr;
            fail(detail::mapErrnoOrIo(error), std::string("Download Failed: fclose failed: ") + std::strerror(error));
            return false;
        }
        outFile = nullptr;
        return true;
    }

    bool closeRemote() {
        if (!fh || !core.ctx) return true;
        const int result = smb2_close(core.ctx, fh);
        fh = nullptr;
        if (result < 0) {
            const std::string message = std::string("Download Failed: smb2_close: ") + smb2_get_error(core.ctx);
            fail(detail::mapSmbResult(result, message), message);
            return false;
        }
        return true;
    }
};

}  // namespace

int64_t smbReadFileAsync(void* context, SmbConnectionManager& manager, const std::string& remotePath,
                         const std::string& localPath, const std::string& taskId,
                         std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    auto* ctx = static_cast<smb2_context*>(context);
    const std::string tempPath = path_util::tempSiblingPath(localPath, taskId.empty() ? "tmp" : taskId);

    DownloadState state;
    state.init();
    state.core.ctx = ctx;
    state.core.cancel = cancel;
    state.core.progressHandler = std::move(progressHandler);
    state.fh = smb2_open(ctx, remotePath.c_str(), O_RDONLY);
    if (!state.fh) {
        const std::string message =
            "Download Failed: Could not open remote file '" + remotePath + "': " + smb2_get_error(ctx);
        SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, message), message);
    }

    smb2_stat_64 stat{};
    if (smb2_fstat(ctx, state.fh, &stat) < 0) {
        const std::string error = smb2_get_error(ctx);
        smb2_close(ctx, state.fh);
        state.fh = nullptr;
        SmbException::raise(SmbErrorCode::Io, "Download Failed: Could not stat '" + remotePath + "': " + error);
    }
    state.core.fileSize = static_cast<int64_t>(stat.smb2_size);

    auto removeTemp = [&] { std::remove(tempPath.c_str()); };
    if (state.core.cancel.cancelled()) {
        state.cleanup();
        SmbException::raise(SmbErrorCode::Cancelled, "Download cancelled");
    }
    std::remove(tempPath.c_str());
    state.outFile = fopen(tempPath.c_str(), "wb");
    if (!state.outFile) {
        state.cleanup();
        SmbException::raiseFromErrno(errno, "Download Failed: Could not create local temp file '" + tempPath + "'");
    }

    if (state.core.fileSize == 0) {
        if (!state.finalizeLocal() || !state.closeRemote()) {
            removeTemp();
            detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
        }
        if (state.core.cancel.cancelled()) {
            removeTemp();
            SmbException::raise(SmbErrorCode::Cancelled, "Download cancelled");
        }
        if (std::rename(tempPath.c_str(), localPath.c_str()) != 0) {
            const int error = errno;
            removeTemp();
            SmbException::raiseFromErrno(error, "Download Failed: rename temp to final failed");
        }
        if (state.core.progressHandler) state.core.progressHandler(0.0, 0.0);
        return 0;
    }

    state.core.chunkSize = detail::capAsyncChunkSize(smb2_get_max_read_size(ctx));
    if (state.core.progressHandler) state.core.progressHandler(0.0, static_cast<double>(state.core.fileSize));
    detail::runTransfer(
        state.core, manager,
        [&] {
            for (int i = 0; i < detail::kMaxInFlight && !state.core.failure.errored; ++i) state.submitNext();
        },
        [&] { return state.done() && state.core.inFlight == 0; }, [&] { return state.stuck(); },
        [&] { state.cancel(); }, [&] { state.fh = nullptr; }, "Download Failed");
    if (state.core.failure.errored) {
        state.cleanup();
        removeTemp();
        detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
    }
    if (!state.finalizeLocal() || !state.closeRemote()) {
        removeTemp();
        detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
    }
    if (state.core.completedBytes != state.core.fileSize) {
        removeTemp();
        SmbException::raise(SmbErrorCode::Io,
                            "Downloaded file size mismatch. Expected: " + std::to_string(state.core.fileSize) +
                                ", Got: " + std::to_string(state.core.completedBytes));
    }
    if (state.core.cancel.cancelled()) {
        removeTemp();
        SmbException::raise(SmbErrorCode::Cancelled, "Download cancelled");
    }
    if (std::rename(tempPath.c_str(), localPath.c_str()) != 0) {
        const int error = errno;
        removeTemp();
        SmbException::raiseFromErrno(error, "Download Failed: rename temp to final failed");
    }
    if (state.core.progressHandler) {
        state.core.progressHandler(static_cast<double>(state.core.fileSize), static_cast<double>(state.core.fileSize));
    }
    return state.core.completedBytes;
}

}  // namespace react_native_smb
