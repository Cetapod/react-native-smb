#include "SmbServerSideCopy.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/smb2-errors.h>

#include "../../connection/SmbConnection.hpp"
#include "../../util/SmbLog.hpp"

namespace react_native_smb::detail {
namespace {

// Within the protocol's default server limits (256 chunks, 1 MiB each, 16 MiB per request).
constexpr uint32_t kChunkLength = 1024 * 1024;
constexpr uint32_t kChunksPerRequest = 16;
constexpr int kRequestsInFlight = 4;
constexpr size_t kResumeKeySize = 24;
constexpr size_t kChunkEntrySize = 24;
constexpr size_t kCopyHeaderSize = kResumeKeySize + 8;  // key + chunk count + reserved
constexpr size_t kCopyResponseSize = 12;                // chunks written, chunk bytes, total bytes

bool isUnsupported(uint32_t status) {
    return status == SMB2_STATUS_NOT_SUPPORTED || status == SMB2_STATUS_INVALID_DEVICE_REQUEST ||
           status == SMB2_STATUS_NOT_IMPLEMENTED || status == SMB2_STATUS_INVALID_PARAMETER;
}

std::string statusText(uint32_t status) {
    const char* text = nterror_to_str(status);
    return text ? text : "unknown";
}

void putU32(uint8_t* out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}

void putU64(uint8_t* out, uint64_t value) {
    for (int i = 0; i < 8; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}

uint32_t getU32(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
}

struct ServerCopy {
    struct Request {
        ServerCopy* owner = nullptr;
        bool busy = false;
        uint64_t length = 0;
        std::vector<uint8_t> input;  // libsmb2 references it until the reply arrives
    };

    TransferCore& core;
    smb2fh* source;
    smb2fh* destination;
    std::shared_ptr<int64_t> totalCopied;
    int64_t totalSize;

    uint8_t resumeKey[kResumeKeySize]{};
    bool keyReady = false;
    bool unsupported = false;
    uint32_t rejectedStatus = 0;
    Request requests[kRequestsInFlight];

    ServerCopy(TransferCore& core, smb2fh* source, smb2fh* destination, std::shared_ptr<int64_t> totalCopied, int64_t totalSize)
        : core(core), source(source), destination(destination), totalCopied(std::move(totalCopied)), totalSize(totalSize) {
        for (auto& request : requests) request.owner = this;
    }

    void fail(SmbErrorCode code, std::string message) { core.failure.fail(code, std::move(message)); }

    void reject(uint32_t status) {
        unsupported = true;
        if (rejectedStatus == 0) rejectedStatus = status;
    }

    bool queueIoctl(uint32_t ctlCode, smb2fh* handle, std::vector<uint8_t>* input, smb2_command_cb cb, void* cbData) {
        smb2_ioctl_request request;
        std::memset(&request, 0, sizeof(request));
        request.ctl_code = ctlCode;
        std::memcpy(request.file_id, *smb2_get_file_id(handle), SMB2_FD_SIZE);
        request.flags = SMB2_0_IOCTL_IS_FSCTL;
        if (input) {
            request.input_count = static_cast<uint32_t>(input->size());
            request.input = input->data();
        }
        smb2_pdu* pdu = smb2_cmd_ioctl_async(core.ctx, &request, cb, cbData);
        if (!pdu) {
            fail(SmbErrorCode::Io, std::string("Copy Failed: could not queue server-side copy: ") + smb2_get_error(core.ctx));
            return false;
        }
        smb2_queue_pdu(core.ctx, pdu);
        ++core.inFlight;
        return true;
    }

    void requestResumeKey() { queueIoctl(SMB2_FSCTL_SRV_REQUEST_RESUME_KEY, source, nullptr, onResumeKey, this); }

    void fill() {
        for (auto& request : requests) {
            if (core.failure.errored || unsupported || core.nextOffset >= static_cast<uint64_t>(core.fileSize)) return;
            if (core.cancel.cancelled()) {
                fail(SmbErrorCode::Cancelled, "Copy cancelled");
                return;
            }
            if (!request.busy) submitCopy(request);
        }
    }

    void submitCopy(Request& request) {
        const uint64_t offset = core.nextOffset;
        const uint64_t length = std::min<uint64_t>(static_cast<uint64_t>(kChunkLength) * kChunksPerRequest,
                                                   static_cast<uint64_t>(core.fileSize) - offset);
        const uint32_t chunks = static_cast<uint32_t>((length + kChunkLength - 1) / kChunkLength);

        request.input.assign(kCopyHeaderSize + kChunkEntrySize * chunks, 0);
        uint8_t* out = request.input.data();
        std::memcpy(out, resumeKey, kResumeKeySize);
        putU32(out + kResumeKeySize, chunks);
        for (uint32_t i = 0; i < chunks; ++i) {
            uint8_t* entry = out + kCopyHeaderSize + kChunkEntrySize * i;
            const uint64_t chunkOffset = offset + static_cast<uint64_t>(i) * kChunkLength;
            putU64(entry, chunkOffset);       // source offset
            putU64(entry + 8, chunkOffset);   // target offset
            putU32(entry + 16, static_cast<uint32_t>(std::min<uint64_t>(kChunkLength, offset + length - chunkOffset)));
        }

        if (!queueIoctl(SMB2_FSCTL_SRV_COPYCHUNK_WRITE, destination, &request.input, onCopy, &request)) return;
        request.busy = true;
        request.length = length;
        core.nextOffset += length;
    }

    static void onResumeKey(smb2_context* smb2, int status, void* commandData, void* privateData) {
        auto& self = *static_cast<ServerCopy*>(privateData);
        auto* reply = static_cast<smb2_ioctl_reply*>(commandData);
        --self.core.inFlight;
        const auto s = static_cast<uint32_t>(status);

        if (s == SMB2_STATUS_SUCCESS && reply && reply->output && reply->output_count >= kResumeKeySize) {
            std::memcpy(self.resumeKey, reply->output, kResumeKeySize);
            self.keyReady = true;
        }
        if (s == SMB2_STATUS_SUCCESS && reply && reply->output) smb2_free_data(smb2, reply->output);

        if (self.core.failure.errored) return;
        if (s != SMB2_STATUS_SUCCESS) {
            if (isUnsupported(s)) {
                self.reject(s);
            } else {
                const std::string message = "Copy Failed: resume key request failed: " + statusText(s);
                self.fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(0, status, message)), message);
            }
            return;
        }
        if (!self.keyReady) {
            self.fail(SmbErrorCode::Io, "Copy Failed: invalid resume key reply");
            return;
        }
        self.fill();
    }

    static void onCopy(smb2_context* smb2, int status, void* commandData, void* privateData) {
        auto& request = *static_cast<Request*>(privateData);
        auto& self = *request.owner;
        auto* reply = static_cast<smb2_ioctl_reply*>(commandData);
        --self.core.inFlight;
        request.busy = false;
        const auto s = static_cast<uint32_t>(status);

        uint32_t written = 0;
        bool parsed = false;
        if (s == SMB2_STATUS_SUCCESS && reply && reply->output) {
            if (reply->output_count >= kCopyResponseSize) {
                written = getU32(static_cast<const uint8_t*>(reply->output) + 8);
                parsed = true;
            }
            smb2_free_data(smb2, reply->output);
        }

        if (self.core.failure.errored) return;
        if (s != SMB2_STATUS_SUCCESS) {
            if (isUnsupported(s)) {
                self.reject(s);
            } else {
                const std::string message = "Copy Failed: server-side copy failed: " + statusText(s);
                self.fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(0, status, message)), message);
            }
            return;
        }
        if (!parsed || written != request.length) {
            self.fail(SmbErrorCode::Io, "Copy Failed: server copied " + std::to_string(written) + " of " +
                                            std::to_string(request.length) + " bytes");
            return;
        }

        self.core.completedBytes += static_cast<int64_t>(request.length);
        if (self.totalCopied) *self.totalCopied += static_cast<int64_t>(request.length);
        self.core.emitProgress(self.totalCopied ? *self.totalCopied : self.core.completedBytes, self.totalSize);
        self.fill();
    }

    bool done() const {
        if (core.inFlight > 0) return false;
        return core.failure.errored || unsupported || core.completedBytes >= core.fileSize;
    }
};

}  // namespace

ServerCopyResult serverSideCopy(TransferCore& core, SmbConnectionManager& manager, smb2fh* source, smb2fh* destination,
                                const std::shared_ptr<int64_t>& totalCopied, int64_t totalSize,
                                const std::function<void()>& clearHandles) {
    ServerCapabilities& capabilities = manager.capabilities();
    if (capabilities.serverSideCopyUnsupported.load()) return ServerCopyResult::Unsupported;

    ServerCopy copy(core, source, destination, totalCopied, totalSize);
    runTransfer(
        core, manager, [&] { copy.requestResumeKey(); }, [&] { return copy.done(); }, [] { return false; },
        [&] { copy.fail(SmbErrorCode::Cancelled, "Copy cancelled"); }, clearHandles, "Copy Failed");

    if (core.failure.errored || !copy.unsupported) return ServerCopyResult::Copied;
    if (core.completedBytes > 0) {
        core.failure.fail(SmbErrorCode::Io, "Copy Failed: server-side copy stopped after " + std::to_string(core.completedBytes) +
                                                " bytes: " + statusText(copy.rejectedStatus));
        return ServerCopyResult::Copied;
    }

    if (!capabilities.serverSideCopyUnsupported.exchange(true)) {
        SMB_LOG_INFO("Copy: server-side copy not supported (%s); copying through the client", statusText(copy.rejectedStatus).c_str());
    }
    core.nextOffset = 0;
    core.lastProgressBytes = 0;
    return ServerCopyResult::Unsupported;
}

}  // namespace react_native_smb::detail
