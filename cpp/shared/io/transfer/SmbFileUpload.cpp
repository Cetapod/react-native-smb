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
#include "../../util/SmbLog.hpp"
#include "../SmbPathUtil.hpp"

namespace react_native_smb {
namespace {

using detail::TransferCore;

enum class RemotePathPresence { Exists, NotFound, Error };

RemotePathPresence remotePathPresence(smb2_context* ctx, const std::string& path, std::string* error = nullptr,
                                      int* code = nullptr) {
    smb2_stat_64 stat{};
    const int result = smb2_stat(ctx, path.c_str(), &stat);
    if (result == 0) return RemotePathPresence::Exists;

    const std::string message = smb2_get_error(ctx);
    const int mapped = static_cast<int>(detail::mapSmbResult(result, message));
    if (mapped == static_cast<int>(SmbErrorCode::NotFound)) return RemotePathPresence::NotFound;
    if (error) *error = message;
    if (code) *code = mapped;
    return RemotePathPresence::Error;
}

bool remotePathExists(smb2_context* ctx, const std::string& path) {
    std::string error;
    int code = static_cast<int>(SmbErrorCode::Io);
    const auto presence = remotePathPresence(ctx, path, &error, &code);
    if (presence == RemotePathPresence::Exists) return true;
    if (presence == RemotePathPresence::NotFound) return false;
    SmbException::raise(code, "Upload Failed: could not stat '" + path + "': " + error);
}

void commitRemoteRename(smb2_context* ctx, const std::string& temp, const std::string& final,
                        const std::string& taskId) {
    if (!remotePathExists(ctx, final)) {
        const int result = smb2_rename(ctx, temp.c_str(), final.c_str());
        if (result < 0) {
            SmbException::raiseFromErrnoResult(
                result, std::string("Upload Failed: smb2_rename temp-to-final: ") + smb2_get_error(ctx));
        }
        return;
    }

    const std::string backup = path_util::backupSiblingPath(final, taskId.empty() ? "tmp" : taskId);
    if (remotePathExists(ctx, backup)) {
        SmbException::raise(SmbErrorCode::AlreadyExists,
                            "Upload Failed: backup path already exists from a prior attempt ('" + backup + "')");
    }
    const int moved = smb2_rename(ctx, final.c_str(), backup.c_str());
    if (moved < 0) {
        SmbException::raiseFromErrnoResult(
            moved, std::string("Upload Failed: could not move existing destination aside: ") + smb2_get_error(ctx));
    }

    const int committed = smb2_rename(ctx, temp.c_str(), final.c_str());
    if (committed < 0) {
        const std::string error = smb2_get_error(ctx);
        const int restored = smb2_rename(ctx, backup.c_str(), final.c_str());
        if (restored < 0) {
            SmbException::raiseFromErrnoResult(committed,
                                               "Upload Failed: smb2_rename temp-to-final failed (" + error +
                                                   ") and restore from backup also failed: " + smb2_get_error(ctx));
        }
        SmbException::raiseFromErrnoResult(committed,
                                           "Upload Failed: smb2_rename temp-to-final after backup: " + error);
    }
    if (smb2_unlink(ctx, backup.c_str()) < 0) SMB_LOG_WARN("Upload backup cleanup failed");
}

struct UploadState {
    struct Slot {
        std::vector<uint8_t> buffer;
        uint64_t offset = 0;
        uint32_t requested = 0;
        uint32_t written = 0;
        bool available = true;
    };

    struct CallbackData {
        UploadState* state;
        int slot;
    };

    TransferCore core;
    smb2fh* fh = nullptr;
    FILE* inFile = nullptr;
    std::string tempPath;
    Slot slots[detail::kMaxInFlight];
    CallbackData callbacks[detail::kMaxInFlight];

    void init() {
        for (int i = 0; i < detail::kMaxInFlight; ++i) callbacks[i] = {this, i};
    }

    void fail(SmbErrorCode code, std::string message) { core.failure.fail(code, std::move(message)); }

    void cancel() { fail(SmbErrorCode::Cancelled, "Upload cancelled"); }

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
        if (fseeko(inFile, static_cast<off_t>(core.nextOffset), SEEK_SET) != 0 ||
            fread(slot.buffer.data(), 1, length, inFile) != length) {
            fail(detail::mapErrnoOrIo(errno), "Upload Failed: could not read from local file");
            return;
        }
        slot.offset = core.nextOffset;
        slot.requested = length;
        slot.written = 0;
        slot.available = false;
        core.nextOffset += length;
        const int result =
            smb2_pwrite_async(core.ctx, fh, slot.buffer.data(), length, slot.offset, writeCallback, &callbacks[index]);
        if (result < 0) {
            const std::string message = std::string("Upload Failed: smb2_pwrite_async: ") + smb2_get_error(core.ctx);
            fail(detail::mapSmbResult(result, message), message);
            slot.available = true;
            core.nextOffset -= length;
            return;
        }
        ++core.inFlight;
    }

    static void writeCallback(smb2_context* ctx, int status, void*, void* callbackData) {
        auto* data = static_cast<CallbackData*>(callbackData);
        auto& state = *data->state;
        auto& slot = state.slots[data->slot];
        --state.core.inFlight;
        if (state.core.failure.errored) {
            slot.available = true;
            return;
        }
        if (status < 0) {
            const std::string message = std::string("Upload Failed: write error: ") + smb2_get_error(ctx);
            state.fail(detail::mapSmbResult(status, message), message);
            slot.available = true;
            return;
        }
        const uint32_t remaining = slot.requested - slot.written;
        if (static_cast<uint64_t>(status) > remaining) {
            state.fail(SmbErrorCode::Io, "Upload Failed: invalid write completion size");
            slot.available = true;
            return;
        }
        slot.written += static_cast<uint32_t>(status);
        state.core.completedBytes += status;
        if (slot.written < slot.requested) {
            if (status == 0) {
                state.fail(SmbErrorCode::Io, "Upload Failed: server accepted zero bytes");
                slot.available = true;
                return;
            }
            const uint32_t tail = slot.requested - slot.written;
            const int result = smb2_pwrite_async(state.core.ctx, state.fh, slot.buffer.data() + slot.written, tail,
                                                 slot.offset + slot.written, writeCallback, callbackData);
            if (result < 0) {
                const std::string message = std::string("Upload Failed: short-write recovery smb2_pwrite_async: ") +
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

    void cleanupOnError() {
        if (fh && core.ctx) {
            smb2_close(core.ctx, fh);
            fh = nullptr;
        }
        if (inFile) {
            fclose(inFile);
            inFile = nullptr;
        }
        if (core.ctx) smb2_unlink(core.ctx, tempPath.c_str());
    }

    bool finalize() {
        if (fh && core.ctx) {
            const int sync = smb2_fsync(core.ctx, fh);
            if (sync < 0) {
                const std::string message = std::string("Upload Failed: smb2_fsync: ") + smb2_get_error(core.ctx);
                fail(detail::mapSmbResult(sync, message), message);
                smb2_close(core.ctx, fh);
                fh = nullptr;
                return false;
            }
            const int close = smb2_close(core.ctx, fh);
            fh = nullptr;
            if (close < 0) {
                const std::string message = std::string("Upload Failed: smb2_close: ") + smb2_get_error(core.ctx);
                fail(detail::mapSmbResult(close, message), message);
                return false;
            }
        }
        if (inFile && fclose(inFile) != 0) {
            const int error = errno;
            inFile = nullptr;
            fail(detail::mapErrnoOrIo(error),
                 std::string("Upload Failed: fclose local input failed: ") + std::strerror(error));
            return false;
        }
        inFile = nullptr;
        return true;
    }
};

}  // namespace

int64_t smbWriteFileAsync(void* context, SmbConnectionManager& manager, const std::string& localPath,
                          const std::string& remotePath, const std::string& taskId,
                          std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    auto* ctx = static_cast<smb2_context*>(context);
    UploadState state;
    state.init();
    state.core.ctx = ctx;
    state.core.cancel = cancel;
    state.core.progressHandler = std::move(progressHandler);
    state.tempPath = path_util::tempSiblingPath(remotePath, taskId.empty() ? "tmp" : taskId);
    state.inFile = fopen(localPath.c_str(), "rb");
    if (!state.inFile) {
        SmbException::raiseFromErrno(errno, "Upload Failed: Could not open local file '" + localPath + "'");
    }

    if (fseeko(state.inFile, 0, SEEK_END) != 0) {
        const int error = errno;
        fclose(state.inFile);
        state.inFile = nullptr;
        SmbException::raiseFromErrno(error, "Upload Failed: Could not determine size of '" + localPath + "'");
    }
    const off_t size = ftello(state.inFile);
    if (size < 0 || fseeko(state.inFile, 0, SEEK_SET) != 0) {
        const int error = errno;
        fclose(state.inFile);
        state.inFile = nullptr;
        SmbException::raiseFromErrno(error, "Upload Failed: Could not determine size of '" + localPath + "'");
    }
    state.core.fileSize = static_cast<int64_t>(size);

    if (state.core.cancel.cancelled()) {
        state.cleanupOnError();
        SmbException::raise(SmbErrorCode::Cancelled, "Upload cancelled");
    }
    smb2_unlink(ctx, state.tempPath.c_str());
    state.fh = smb2_open(ctx, state.tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    if (!state.fh) {
        const std::string message =
            "Upload Failed: Could not create remote temp '" + state.tempPath + "': " + smb2_get_error(ctx);
        fclose(state.inFile);
        state.inFile = nullptr;
        SmbException::raise(SmbErrorMapper::fromErrnoOrMessage(0, message), message);
    }
    if (state.core.fileSize == 0) {
        if (!state.finalize()) {
            state.cleanupOnError();
            detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
        }
        if (state.core.cancel.cancelled()) {
            state.cleanupOnError();
            SmbException::raise(SmbErrorCode::Cancelled, "Upload cancelled");
        }
        try {
            commitRemoteRename(ctx, state.tempPath, remotePath, taskId);
        } catch (...) {
            smb2_unlink(ctx, state.tempPath.c_str());
            throw;
        }
        if (state.core.progressHandler) state.core.progressHandler(0.0, 0.0);
        return 0;
    }
    state.core.chunkSize = detail::capAsyncChunkSize(smb2_get_max_write_size(ctx));
    if (state.core.progressHandler) state.core.progressHandler(0.0, static_cast<double>(state.core.fileSize));
    detail::runTransfer(
        state.core, manager,
        [&] {
            for (int i = 0; i < detail::kMaxInFlight && !state.core.failure.errored; ++i) state.submitNext();
        },
        [&] { return state.done() && state.core.inFlight == 0; }, [&] { return state.stuck(); },
        [&] { state.cancel(); }, [&] { state.fh = nullptr; }, "Upload Failed");
    if (state.core.failure.errored) {
        state.cleanupOnError();
        detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
    }
    if (!state.finalize()) {
        smb2_unlink(ctx, state.tempPath.c_str());
        detail::raiseTransferError(state.core.failure.code, state.core.failure.message);
    }
    if (state.core.completedBytes != state.core.fileSize) {
        smb2_unlink(ctx, state.tempPath.c_str());
        SmbException::raise(SmbErrorCode::Io,
                            "Uploaded file size mismatch. Expected: " + std::to_string(state.core.fileSize) +
                                ", Uploaded: " + std::to_string(state.core.completedBytes));
    }
    if (state.core.cancel.cancelled()) {
        smb2_unlink(ctx, state.tempPath.c_str());
        SmbException::raise(SmbErrorCode::Cancelled, "Upload cancelled");
    }
    try {
        commitRemoteRename(ctx, state.tempPath, remotePath, taskId);
    } catch (...) {
        smb2_unlink(ctx, state.tempPath.c_str());
        throw;
    }
    if (state.core.progressHandler) {
        state.core.progressHandler(static_cast<double>(state.core.fileSize), static_cast<double>(state.core.fileSize));
    }
    return state.core.completedBytes;
}

}  // namespace react_native_smb
