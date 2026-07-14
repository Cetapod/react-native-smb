#include "SmbFileIO.hpp"  // sibling

#include <fcntl.h>
#include <poll.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "../connection/SmbConnection.hpp"
#include "../core/CancellationToken.hpp"
#include "../util/SmbErrorMapper.hpp"
#include "../util/SmbLog.hpp"

namespace react_native_smb {

// Progress coalescing: emit only when >=256 KiB or >=0.5% have elapsed since
// the last emission. Declared here so both the legacy sync path and the
// async pipelined paths can use it.
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

int64_t smbReadFile(void* ctxVoid, const std::string& remotePath, const std::string& localPath, std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    smb2fh* fh = smb2_open(ctx, remotePath.c_str(), O_RDONLY);
    if (!fh) {
        std::string error = smb2_get_error(ctx);
        std::string msg = "Download Failed: Could not open remote file for reading: " + remotePath + ". Error: " + error;
        throw std::runtime_error(msg);
    }

    struct smb2_stat_64 stat;
    const int fstatResult = smb2_fstat(ctx, fh, &stat);
    if (fstatResult < 0) {
        smb2_close(ctx, fh);
        std::string error = smb2_get_error(ctx);
        std::string msg = "Download Failed: Could not get stats for file: " + remotePath + ". Error: " + error;
        throw std::runtime_error(msg);
    }

    int64_t fileSize = static_cast<int64_t>(stat.smb2_size);
    int64_t totalBytesRead = 0;

    std::ofstream localFile(localPath, std::ios::binary | std::ios::trunc);
    if (!localFile.is_open()) {
        smb2_close(ctx, fh);
        std::string msg = "Download Failed: Could not create local file at: " + localPath + ". Check permissions.";
        throw std::runtime_error(msg);
    }

    auto cleanup = [&]() {
        localFile.close();
        smb2_close(ctx, fh);
    };

    try {
        constexpr size_t BUFFER_SIZE = 1024 * 1024;
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        ssize_t bytesRead;

        if (fileSize > 0) {
            if (progressHandler) {
                progressHandler(0.0, static_cast<double>(fileSize));
            }

            int64_t lastEmittedDl = 0;
            while ((bytesRead = smb2_read(ctx, fh, buffer.data(), BUFFER_SIZE)) > 0) {
                // Check for cancellation
                if (cancel.cancelled()) {
                    cleanup();
                    std::remove(localPath.c_str());
                    return 0;
                }

                localFile.write(reinterpret_cast<const char*>(buffer.data()), bytesRead);
                if (localFile.fail()) {
                    std::string msg = "Failed to write to local file '" + localPath + "'";
                    throw std::runtime_error(msg);
                }

                totalBytesRead += bytesRead;

                if (progressHandler && shouldEmitProgress(totalBytesRead, fileSize, lastEmittedDl)) {
                    progressHandler(static_cast<double>(totalBytesRead), static_cast<double>(fileSize));
                }

                if (totalBytesRead > fileSize) {
                    std::string msg = "Read more bytes than expected file size";
                    throw std::runtime_error(msg);
                }
            }

            if (bytesRead < 0) {
                std::string error = smb2_get_error(ctx);
                std::string msg = "Failed to read from remote file '" + remotePath + "': " + error;
                throw std::runtime_error(msg);
            }

            localFile.flush();
            if (localFile.fail()) {
                std::string msg = "Failed to flush data to local file '" + localPath + "'";
                throw std::runtime_error(msg);
            }
        }

        if (totalBytesRead != fileSize) {
            std::string msg = "Downloaded file size mismatch. Expected: " + std::to_string(fileSize) + ", Got: " + std::to_string(totalBytesRead);
            throw std::runtime_error(msg);
        }

        cleanup();

        if (progressHandler) {
            progressHandler(static_cast<double>(fileSize), static_cast<double>(fileSize));
        }
    } catch (const std::exception& e) {
        cleanup();
        try {
            std::remove(localPath.c_str());
        } catch (const std::exception& removeError) {
            // Failed to remove incomplete file, ignoring to preserve original error
        }
        throw;
    }

    return totalBytesRead;
}

int64_t smbWriteFile(void* ctxVoid, const std::string& localPath, const std::string& remotePath, std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    std::ifstream localFile(localPath, std::ios::binary | std::ios::ate);
    if (!localFile.is_open()) {
        std::string msg = "Upload Failed: Could not open local file for reading: " + localPath;
        throw std::runtime_error(msg);
    }

    std::streamsize localFileSize = localFile.tellg();
    localFile.seekg(0, std::ios::beg);

    if (localFileSize < 0) {
        std::string msg = "Upload Failed: Could not determine size of local file: " + localPath;
        throw std::runtime_error(msg);
    }

    smb2fh* fh = smb2_open(ctx, remotePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    if (!fh) {
        std::string error = smb2_get_error(ctx);
        std::string msg = "Upload Failed: Could not create remote file: " + remotePath + ". Error: " + error;
        throw std::runtime_error(msg);
    }

    auto cleanup = [&]() {
        localFile.close();
        smb2_close(ctx, fh);
    };

    try {
        constexpr size_t BUFFER_SIZE = 1024 * 1024;
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        int64_t totalBytesWritten = 0;

        if (progressHandler) {
            progressHandler(0.0, static_cast<double>(localFileSize));
        }

        while (localFile.read(reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE) || localFile.gcount() > 0) {
            // Check for cancellation
            if (cancel.cancelled()) {
                cleanup();
                smb2_unlink(ctx, remotePath.c_str());
                return 0;
            }

            auto bytesToWrite = static_cast<uint32_t>(localFile.gcount());
            if (localFile.bad()) {
                std::string msg = "Error reading from local file '" + localPath + "'";
                throw std::runtime_error(msg);
            }

            ssize_t bytesWritten = smb2_write(ctx, fh, buffer.data(), bytesToWrite);
            if (bytesWritten < 0) {
                std::string error = smb2_get_error(ctx);
                std::string msg = "Failed to write to remote file '" + remotePath + "': " + error;
                throw std::runtime_error(msg);
            }

            if (static_cast<uint32_t>(bytesWritten) != bytesToWrite) {
                std::string msg = "Incomplete write to remote file '" + remotePath + "'";
                throw std::runtime_error(msg);
            }

            totalBytesWritten += bytesWritten;

            if (progressHandler) {
                progressHandler(static_cast<double>(totalBytesWritten), static_cast<double>(localFileSize));
            }

            if (totalBytesWritten > localFileSize) {
                std::string msg = "Written more bytes than local file size";
                throw std::runtime_error(msg);
            }
        }

        if (smb2_fsync(ctx, fh) < 0) {
        }

        if (totalBytesWritten != localFileSize) {
            std::string msg = "Uploaded file size mismatch. Expected: " + std::to_string(localFileSize) + ", Uploaded: " + std::to_string(totalBytesWritten);
            throw std::runtime_error(msg);
        }

        if (progressHandler) {
            progressHandler(static_cast<double>(localFileSize), static_cast<double>(localFileSize));
        }

        cleanup();
        return totalBytesWritten;
    } catch (const std::exception& e) {
        cleanup();
        try {
            smb2_unlink(ctx, remotePath.c_str());
        } catch (const std::exception& unlinkError) {
        }

        throw;
    }
}

int64_t smbCopyFile(void* ctxVoid, const std::string& fromPath, const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
                    std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    // Open source file
    smb2fh* sourceFh = smb2_open(ctx, fromPath.c_str(), O_RDONLY);
    if (!sourceFh) {
        std::string error = smb2_get_error(ctx);
        std::string msg = "Failed to open source file '" + fromPath + "': " + error;
        throw std::runtime_error(msg);
    }

    // Open destination file
    smb2fh* destFh = smb2_open(ctx, toPath.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (!destFh) {
        smb2_close(ctx, sourceFh);
        std::string error = smb2_get_error(ctx);
        std::string msg = "Failed to create destination file '" + toPath + "': " + error;
        throw std::runtime_error(msg);
    }

    auto cleanup = [&]() {
        smb2_close(ctx, sourceFh);
        smb2_close(ctx, destFh);
    };

    try {
        constexpr size_t BUFFER_SIZE = 64 * 1024;
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        int64_t fileBytescopied = 0;

        ssize_t bytesRead;
        while ((bytesRead = smb2_read(ctx, sourceFh, buffer.data(), BUFFER_SIZE)) > 0) {
            // Check for cancellation
            if (cancel.cancelled()) {
                cleanup();
                smb2_unlink(ctx, toPath.c_str());
                return 0;
            }

            ssize_t bytesWritten = smb2_write(ctx, destFh, buffer.data(), static_cast<uint32_t>(bytesRead));
            if (bytesWritten < 0) {
                std::string error = smb2_get_error(ctx);
                std::string msg = "Failed to write to destination file '" + toPath + "': " + error;
                throw std::runtime_error(msg);
            }
            if (bytesWritten != bytesRead) {
                std::string msg = "Incomplete write to destination file '" + toPath + "'";
                throw std::runtime_error(msg);
            }

            fileBytescopied += bytesWritten;

            // Update cumulative progress if counter is provided
            if (totalBytesCopied) {
                *totalBytesCopied += bytesWritten;
            }

            // Report progress if handler is provided
            if (progressHandler && totalSize > 0) {
                int64_t currentProgress = totalBytesCopied ? *totalBytesCopied : fileBytescopied;
                progressHandler(static_cast<double>(currentProgress), static_cast<double>(totalSize));
            }
        }

        if (bytesRead < 0) {
            std::string error = smb2_get_error(ctx);
            std::string msg = "Failed to read from source file '" + fromPath + "': " + error;
            throw std::runtime_error(msg);
        }

        // Sync the destination file
        if (smb2_fsync(ctx, destFh) < 0) {
            // Sync failed, but continue
        }

        cleanup();

        return fileBytescopied;
    } catch (const std::exception& e) {
        cleanup();
        // Try to delete the incomplete destination file
        try {
            smb2_unlink(ctx, toPath.c_str());
        } catch (const std::exception& unlinkError) {
        }
        throw;
    }
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
    std::string errorMsg;

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
        if (!errored) {
            errored = true;
            errorMsg = "Download cancelled";
        }
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
            errored = true;
            errorMsg = std::string("Download Failed: smb2_pread_async: ") + smb2_get_error(ctx);
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
            st->errored = true;
            st->errorMsg = std::string("Download Failed: read error: ") + smb2_get_error(smb2);
            if (slotIdx >= 0) st->slots[slotIdx].available = true;
            return;
        }
        if (slotIdx < 0) {
            st->errored = true;
            st->errorMsg = "Download Failed: internal slot lookup failed";
            return;
        }

        auto& slot = st->slots[slotIdx];

        // File offset where THIS returned chunk starts (slot may already
        // hold `received` bytes from prior short-read recoveries).
        const uint64_t writeOffset = slot.baseOffset + slot.received;

        // ── Persist the received bytes to local file ──
        if (st->outFile && status > 0) {
            if (fseeko(st->outFile, static_cast<off_t>(writeOffset), SEEK_SET) != 0 || fwrite(cbd->buf, 1, static_cast<size_t>(status), st->outFile) != static_cast<size_t>(status)) {
                st->errored = true;
                st->errorMsg = "Download Failed: local file write error";
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
                // Zero-byte completion before fileSize → unrecoverable.
                st->errored = true;
                st->errorMsg = "Download Failed: unexpected EOF (zero-byte read)";
                slot.available = true;
                return;
            }
            uint32_t tailLen = slot.requested - slot.received;
            uint64_t tailOffset = slot.baseOffset + slot.received;
            SMB_LOG("AsyncReadState: short read got=%d requested=%u at offset=%llu — re-issuing tail %u bytes at %llu", status, slot.requested, static_cast<unsigned long long>(slot.baseOffset),
                    tailLen, static_cast<unsigned long long>(tailOffset));
            int ret = smb2_pread_async(st->ctx, st->fh, slot.buf.data() + slot.received, tailLen, tailOffset, readCb, st);
            if (ret < 0) {
                st->errored = true;
                st->errorMsg = std::string("Download Failed: short-read recovery smb2_pread_async: ") + smb2_get_error(st->ctx);
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
            fflush(outFile);
            fclose(outFile);
            outFile = nullptr;
        }
        if (fh && ctx) {
            smb2_close(ctx, fh);
            fh = nullptr;
        }
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
    std::string errorMsg;

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
            errored = true;
            errorMsg = "Upload cancelled";
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
            errored = true;
            errorMsg = "Upload Failed: could not read from local file";
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
            errored = true;
            errorMsg = std::string("Upload Failed: smb2_pwrite_async: ") + smb2_get_error(ctx);
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
            st->errored = true;
            st->errorMsg = std::string("Upload Failed: write error: ") + smb2_get_error(smb2);
            if (slotIdx >= 0) st->slots[slotIdx].available = true;
            return;
        }
        if (slotIdx < 0) {
            st->errored = true;
            st->errorMsg = "Upload Failed: internal slot lookup failed";
            return;
        }

        auto& slot = st->slots[slotIdx];
        slot.written += static_cast<uint32_t>(status);
        st->bytesCompleted += status;

        // ── L1: short-write recovery ──
        if (slot.written < slot.requested) {
            if (status == 0) {
                st->errored = true;
                st->errorMsg = "Upload Failed: server accepted zero bytes";
                slot.available = true;
                return;
            }
            uint32_t tailLen = slot.requested - slot.written;
            uint64_t tailOffset = slot.baseOffset + slot.written;
            SMB_LOG("AsyncWriteState: short write got=%d requested=%u at offset=%llu — re-issuing tail %u bytes at %llu", status, slot.requested, static_cast<unsigned long long>(slot.baseOffset),
                    tailLen, static_cast<unsigned long long>(tailOffset));
            int ret = smb2_pwrite_async(st->ctx, st->fh, slot.buf.data() + slot.written, tailLen, tailOffset, writeCb, st);
            if (ret < 0) {
                st->errored = true;
                st->errorMsg = std::string("Upload Failed: short-write recovery smb2_pwrite_async: ") + smb2_get_error(st->ctx);
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
            smb2_fsync(ctx, fh);
            smb2_close(ctx, fh);
            fh = nullptr;
        }
        if (inFile) {
            fclose(inFile);
            inFile = nullptr;
        }
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

template <typename DoneFn, typename TickFn>
bool drivePollLoop(smb2_context* ctx, DoneFn&& done, TickFn&& tick) {
    struct pollfd pfd;
    while (!done()) {
        tick();
        if (done()) break;
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
            return false;
        }
        if (ret == 0) continue;  // timeout, retry
        int svc = smb2_service(ctx, pfd.revents);
        if (svc < 0) return false;
    }
    return true;
}

// Drive the smb2 poll/service loop on the calling thread until `done()` is
// true. Used by async read/write/copy paths to pump pipelined chunks.
// Returns false if poll() failed with anything other than EINTR.
template <typename DoneFn>
bool drivePollLoop(smb2_context* ctx, DoneFn&& done) {
    return drivePollLoop(ctx, std::forward<DoneFn>(done), []() {});
}

}  // anonymous namespace

int64_t smbReadFileAsync(void* ctxVoid, const std::string& remotePath, const std::string& localPath, std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    AsyncReadState state;
    state.ctx = ctx;
    state.progressHandler = std::move(progressHandler);
    state.cancel = cancel;
    state.localPath = localPath;

    // Open remote file
    state.fh = smb2_open(ctx, remotePath.c_str(), O_RDONLY);
    if (!state.fh) {
        throw std::runtime_error("Download Failed: Could not open remote file '" + remotePath + "': " + smb2_get_error(ctx));
    }

    // Get file size
    struct smb2_stat_64 st;
    if (smb2_fstat(ctx, state.fh, &st) < 0) {
        std::string err = smb2_get_error(ctx);
        smb2_close(ctx, state.fh);
        throw std::runtime_error("Download Failed: Could not stat '" + remotePath + "': " + err);
    }
    state.fileSize = static_cast<int64_t>(st.smb2_size);

    // Handle empty file
    if (state.fileSize == 0) {
        FILE* f = fopen(localPath.c_str(), "wb");
        if (f) fclose(f);
        smb2_close(ctx, state.fh);
        state.fh = nullptr;
        if (state.progressHandler) state.progressHandler(0.0, 0.0);
        return 0;
    }

    // Determine chunk size from server negotiation, capped for cancel responsiveness.
    state.chunkSize = capAsyncChunkSize(smb2_get_max_read_size(ctx));

    // Open local file for random-access writing
    state.outFile = fopen(localPath.c_str(), "wb");
    if (!state.outFile) {
        smb2_close(ctx, state.fh);
        state.fh = nullptr;
        throw std::runtime_error("Download Failed: Could not create local file '" + localPath + "'");
    }

    if (state.progressHandler) state.progressHandler(0.0, static_cast<double>(state.fileSize));

    // Kick off the first batch of async reads
    for (int i = 0; i < kMaxInFlight && !state.errored; i++) {
        state.submitNextRead();
    }

    // Drive the poll loop until all chunks complete (or error / cancel / stuck).
    // isStuck() is a defence-in-depth check: if a callback path ever fails
    // to recover from a short transfer, the loop will exit instead of hanging.
    bool pollOk = drivePollLoop(
        ctx, [&]() { return (state.isDone() && state.inFlight == 0) || state.isStuck(); },
        [&]() {
            if (cancel.cancelled() && !state.errored) {
                state.markCancelled();
            }
        });
    if (state.isStuck()) {
        SMB_LOG("smbReadFileAsync: stuck-state safety net triggered (bytesCompleted=%lld fileSize=%lld)", static_cast<long long>(state.bytesCompleted), static_cast<long long>(state.fileSize));
        state.errored = true;
        state.errorMsg = "Download Failed: I/O pipeline stalled (unrecoverable short transfer)";
    }
    if (!pollOk && !state.errored) {
        state.errored = true;
        state.errorMsg = std::string("Download Failed: poll/service: ") + smb2_get_error(ctx);
    }

    state.cleanup();

    if (state.errored) {
        std::remove(localPath.c_str());
        throw std::runtime_error(state.errorMsg);
    }
    if (state.bytesCompleted != state.fileSize) {
        std::remove(localPath.c_str());
        throw std::runtime_error("Downloaded file size mismatch. Expected: " + std::to_string(state.fileSize) + ", Got: " + std::to_string(state.bytesCompleted));
    }

    if (state.progressHandler) state.progressHandler(static_cast<double>(state.fileSize), static_cast<double>(state.fileSize));
    return state.bytesCompleted;
}

int64_t smbWriteFileAsync(void* ctxVoid, const std::string& localPath, const std::string& remotePath, std::function<void(double, double)> progressHandler, const CancellationToken& cancel) {
    smb2_context* ctx = static_cast<smb2_context*>(ctxVoid);

    AsyncWriteState state;
    state.ctx = ctx;
    state.progressHandler = std::move(progressHandler);
    state.cancel = cancel;
    state.remotePath = remotePath;

    // Open local file for reading and measure size.
    state.inFile = fopen(localPath.c_str(), "rb");
    if (!state.inFile) {
        throw std::runtime_error("Upload Failed: Could not open local file '" + localPath + "'");
    }
    fseeko(state.inFile, 0, SEEK_END);
    int64_t localFileSize = static_cast<int64_t>(ftello(state.inFile));
    fseeko(state.inFile, 0, SEEK_SET);
    if (localFileSize < 0) {
        fclose(state.inFile);
        state.inFile = nullptr;
        throw std::runtime_error("Upload Failed: Could not determine size of '" + localPath + "'");
    }
    state.fileSize = localFileSize;

    // Handle empty file
    if (state.fileSize == 0) {
        smb2fh* fh = smb2_open(ctx, remotePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        if (fh) smb2_close(ctx, fh);
        fclose(state.inFile);
        state.inFile = nullptr;
        if (state.progressHandler) state.progressHandler(0.0, 0.0);
        return 0;
    }

    // Open remote file for writing
    state.fh = smb2_open(ctx, remotePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    if (!state.fh) {
        std::string err = smb2_get_error(ctx);
        fclose(state.inFile);
        state.inFile = nullptr;
        throw std::runtime_error("Upload Failed: Could not create remote file '" + remotePath + "': " + err);
    }

    // Determine chunk size from server negotiation, capped for cancel responsiveness.
    state.chunkSize = capAsyncChunkSize(smb2_get_max_write_size(ctx));

    if (state.progressHandler) state.progressHandler(0.0, static_cast<double>(state.fileSize));

    // Kick off the first batch of async writes
    for (int i = 0; i < kMaxInFlight && !state.errored; i++) {
        state.submitNextWrite();
    }

    bool pollOk = drivePollLoop(
        ctx, [&]() { return (state.isDone() && state.inFlight == 0) || state.isStuck(); },
        [&]() {
            if (cancel.cancelled() && !state.errored) {
                state.errored = true;
                state.errorMsg = "Upload cancelled";
            }
        });
    if (state.isStuck()) {
        SMB_LOG("smbWriteFileAsync: stuck-state safety net triggered (bytesCompleted=%lld fileSize=%lld)", static_cast<long long>(state.bytesCompleted), static_cast<long long>(state.fileSize));
        state.errored = true;
        state.errorMsg = "Upload Failed: I/O pipeline stalled (unrecoverable short transfer)";
    }
    if (!pollOk && !state.errored) {
        state.errored = true;
        state.errorMsg = std::string("Upload Failed: poll/service: ") + smb2_get_error(ctx);
    }

    if (state.errored) {
        state.cleanupOnError();
        throw std::runtime_error(state.errorMsg);
    }

    state.cleanup();

    if (state.bytesCompleted != state.fileSize) {
        smb2_unlink(ctx, remotePath.c_str());
        throw std::runtime_error("Uploaded file size mismatch. Expected: " + std::to_string(state.fileSize) + ", Uploaded: " + std::to_string(state.bytesCompleted));
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
    std::string errorMsg;

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
            errored = true;
            errorMsg = "Copy cancelled";
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
            errored = true;
            errorMsg = std::string("Copy Failed: smb2_pread_async: ") + smb2_get_error(ctx);
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
            errored = true;
            errorMsg = std::string("Copy Failed: smb2_pwrite_async: ") + smb2_get_error(ctx);
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
            st->errored = true;
            st->errorMsg = std::string("Copy Failed: read error: ") + smb2_get_error(smb2);
            st->slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }

        auto& slot = st->slots[slotIdx];
        slot.read += static_cast<uint32_t>(status);

        // ── L1: short-read recovery (re-issue tail on same slot) ──
        if (slot.read < slot.requested) {
            if (status == 0) {
                st->errored = true;
                st->errorMsg = "Copy Failed: unexpected EOF on source (zero-byte read)";
                slot.state = ChunkSlot::Idle;
                return;
            }
            uint32_t tailLen = slot.requested - slot.read;
            uint64_t tailOffset = slot.baseOffset + slot.read;
            SMB_LOG("AsyncCopyState: short read got=%d requested=%u at offset=%llu — re-issuing tail %u bytes at %llu", status, slot.requested, static_cast<unsigned long long>(slot.baseOffset),
                    tailLen, static_cast<unsigned long long>(tailOffset));
            int ret = smb2_pread_async(st->ctx, st->srcFh, slot.buf.data() + slot.read, tailLen, tailOffset, readCb, &st->cbDatas[slotIdx]);
            if (ret < 0) {
                st->errored = true;
                st->errorMsg = std::string("Copy Failed: short-read recovery smb2_pread_async: ") + smb2_get_error(st->ctx);
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
            st->errored = true;
            st->errorMsg = std::string("Copy Failed: write error: ") + smb2_get_error(smb2);
            st->slots[slotIdx].state = ChunkSlot::Idle;
            return;
        }

        auto& slot = st->slots[slotIdx];
        slot.written += static_cast<uint32_t>(status);

        // ── L1: short-write recovery (re-issue tail on same slot) ──
        if (slot.written < slot.read) {
            if (status == 0) {
                st->errored = true;
                st->errorMsg = "Copy Failed: destination accepted zero bytes";
                slot.state = ChunkSlot::Idle;
                return;
            }
            SMB_LOG("AsyncCopyState: short write got=%d total=%u at offset=%llu — re-issuing tail %u bytes", status, slot.read, static_cast<unsigned long long>(slot.baseOffset),
                    slot.read - slot.written);
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
            smb2_fsync(ctx, dstFh);
            smb2_close(ctx, dstFh);
            dstFh = nullptr;
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

int64_t smbCopyFileAsync(void* ctxVoid, const std::string& fromPath, const std::string& toPath, std::shared_ptr<int64_t> totalBytesCopied, int64_t totalSize,
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
        std::string err = smb2_get_error(ctx);
        throw std::runtime_error("Copy Failed: Could not open source '" + fromPath + "': " + err);
    }

    // Size source.
    struct smb2_stat_64 st;
    if (smb2_fstat(ctx, state.srcFh, &st) < 0) {
        std::string err = smb2_get_error(ctx);
        smb2_close(ctx, state.srcFh);
        throw std::runtime_error("Copy Failed: Could not stat source '" + fromPath + "': " + err);
    }
    state.fileSize = static_cast<int64_t>(st.smb2_size);

    // Open destination (exclusive create).
    state.dstFh = smb2_open(ctx, toPath.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (!state.dstFh) {
        std::string err = smb2_get_error(ctx);
        smb2_close(ctx, state.srcFh);
        state.srcFh = nullptr;
        throw std::runtime_error("Copy Failed: Could not create destination '" + toPath + "': " + err);
    }

    // Handle empty file: nothing to read/write, just close.
    if (state.fileSize == 0) {
        smb2_fsync(ctx, state.dstFh);
        smb2_close(ctx, state.dstFh);
        smb2_close(ctx, state.srcFh);
        state.srcFh = state.dstFh = nullptr;
        return 0;
    }

    state.chunkSize = capAsyncChunkSize(smb2_get_max_read_size(ctx));
    uint32_t maxW = capAsyncChunkSize(smb2_get_max_write_size(ctx));
    if (maxW < state.chunkSize) state.chunkSize = maxW;

    // Kick off first batch of reads.
    for (int i = 0; i < kMaxInFlight && !state.errored; i++) {
        state.submitNextRead();
    }

    bool pollOk = drivePollLoop(
        ctx, [&]() { return (state.isDone() && state.inFlight == 0) || state.isStuck(); },
        [&]() {
            if (cancel.cancelled() && !state.errored) {
                state.errored = true;
                state.errorMsg = "Copy cancelled";
            }
        });
    if (state.isStuck()) {
        SMB_LOG("smbCopyFileAsync: stuck-state safety net triggered (bytesCopied=%lld fileSize=%lld)", static_cast<long long>(state.bytesCopied), static_cast<long long>(state.fileSize));
        state.errored = true;
        state.errorMsg = "Copy Failed: I/O pipeline stalled (unrecoverable short transfer)";
    }
    if (!pollOk && !state.errored) {
        state.errored = true;
        state.errorMsg = std::string("Copy Failed: poll/service: ") + smb2_get_error(ctx);
    }

    if (state.errored) {
        state.cleanupOnError();
        throw std::runtime_error(state.errorMsg);
    }

    state.cleanup();

    if (state.bytesCopied != state.fileSize) {
        smb2_unlink(ctx, toPath.c_str());
        throw std::runtime_error("Copied file size mismatch. Expected: " + std::to_string(state.fileSize) + ", Got: " + std::to_string(state.bytesCopied));
    }

    return state.bytesCopied;
}

}  // namespace react_native_smb
