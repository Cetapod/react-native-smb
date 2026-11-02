#include "SmbServerSideCopy.hpp"

#include <string>
#include <vector>

#include "../../connection/SmbConnection.hpp"
#include "../../util/SmbLog.hpp"
#include "SmbCopyChunk.hpp"

namespace react_native_smb::detail {
namespace {

namespace cc = copy_chunk;

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

    uint8_t resumeKey[cc::kResumeKeySize]{};
    bool unsupported = false;
    uint32_t rejectedStatus = 0;
    Request requests[cc::kRequestsInFlight];

    ServerCopy(TransferCore& core, smb2fh* source, smb2fh* destination, std::shared_ptr<int64_t> totalCopied, int64_t totalSize)
        : core(core), source(source), destination(destination), totalCopied(std::move(totalCopied)), totalSize(totalSize) {
        for (auto& request : requests) request.owner = this;
    }

    void fail(SmbErrorCode code, std::string message) { core.failure.fail(code, std::move(message)); }

    // A rejection only means "unsupported"; otherwise it fails the copy.
    void reject(uint32_t status, const std::string& what) {
        if (cc::isUnsupported(status)) {
            unsupported = true;
            if (rejectedStatus == 0) rejectedStatus = status;
            return;
        }
        const std::string message = "Copy Failed: " + what + ": " + cc::statusText(status);
        fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(0, static_cast<int>(status), message)), message);
    }

    bool queue(uint32_t ctlCode, smb2fh* handle, std::vector<uint8_t>* input, smb2_command_cb cb, void* cbData) {
        if (!cc::queueFsctl(core.ctx, ctlCode, handle, input, cb, cbData)) {
            fail(SmbErrorCode::Io, std::string("Copy Failed: could not queue server-side copy: ") + smb2_get_error(core.ctx));
            return false;
        }
        ++core.inFlight;
        return true;
    }

    void requestResumeKey() { queue(SMB2_FSCTL_SRV_REQUEST_RESUME_KEY, source, nullptr, onResumeKey, this); }

    void fill() {
        for (auto& request : requests) {
            if (core.failure.errored || unsupported || core.nextOffset >= static_cast<uint64_t>(core.fileSize)) return;
            if (core.cancel.cancelled()) {
                fail(SmbErrorCode::Cancelled, "Copy cancelled");
                return;
            }
            if (request.busy) continue;

            const uint64_t length = cc::requestLength(static_cast<uint64_t>(core.fileSize), core.nextOffset);
            cc::encodeRequest(resumeKey, core.nextOffset, length, request.input);
            if (!queue(SMB2_FSCTL_SRV_COPYCHUNK_WRITE, destination, &request.input, onCopy, &request)) return;
            request.busy = true;
            request.length = length;
            core.nextOffset += length;
        }
    }

    static void onResumeKey(smb2_context* smb2, int status, void* commandData, void* privateData) {
        auto& self = *static_cast<ServerCopy*>(privateData);
        --self.core.inFlight;
        const auto s = static_cast<uint32_t>(status);
        const bool keyReady = s == SMB2_STATUS_SUCCESS && cc::takeResumeKey(smb2, commandData, self.resumeKey);

        if (self.core.failure.errored) return;
        if (s != SMB2_STATUS_SUCCESS) {
            self.reject(s, "resume key request failed");
            return;
        }
        if (!keyReady) {
            self.fail(SmbErrorCode::Io, "Copy Failed: invalid resume key reply");
            return;
        }
        self.fill();
    }

    static void onCopy(smb2_context* smb2, int status, void* commandData, void* privateData) {
        auto& request = *static_cast<Request*>(privateData);
        auto& self = *request.owner;
        --self.core.inFlight;
        request.busy = false;
        const auto s = static_cast<uint32_t>(status);
        const int64_t written = s == SMB2_STATUS_SUCCESS ? cc::takeBytesWritten(smb2, commandData) : -1;

        if (self.core.failure.errored) return;
        if (s != SMB2_STATUS_SUCCESS) {
            self.reject(s, "server-side copy failed");
            return;
        }
        if (written != static_cast<int64_t>(request.length)) {
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
                                                " bytes: " + cc::statusText(copy.rejectedStatus));
        return ServerCopyResult::Copied;
    }

    if (!capabilities.serverSideCopyUnsupported.exchange(true)) {
        SMB_LOG_INFO("Copy: server-side copy not supported (%s); copying through the client", cc::statusText(copy.rejectedStatus).c_str());
    }
    core.nextOffset = 0;
    core.lastProgressBytes = 0;
    return ServerCopyResult::Unsupported;
}

}  // namespace react_native_smb::detail
