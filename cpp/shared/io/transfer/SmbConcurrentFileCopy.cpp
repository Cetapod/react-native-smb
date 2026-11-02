#include "SmbConcurrentFileCopy.hpp"

#include <fcntl.h>

#include <cerrno>
#include <deque>
#include <utility>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "../PipelinedOps.hpp"
#include "SmbCopyChunk.hpp"
#include "SmbTransferCore.hpp"

namespace react_native_smb::detail {
namespace {

namespace cc = copy_chunk;

class ConcurrentCopy {
   public:
    ConcurrentCopy(PipelinedOps& ops, const std::vector<FileCopyJob>& files, size_t maxFiles, std::shared_ptr<int64_t> totalCopied,
                   int64_t totalSize, std::function<void(double, double)> onProgress)
        : ops_(ops), files_(files), maxFiles_(maxFiles), totalCopied_(std::move(totalCopied)), totalSize_(totalSize) {
        progress_.progressHandler = std::move(onProgress);
    }

    void start() { startMore(); }

   private:
    struct Request {
        bool busy = false;
        uint64_t length = 0;
        std::vector<uint8_t> input;  // libsmb2 references it until the reply arrives
    };

    struct Job {
        const FileCopyJob* file = nullptr;
        smb2fh* source = nullptr;
        smb2fh* destination = nullptr;
        bool destinationCreated = false;
        smb2_stat_64 stat{};
        bool statReady = false;
        uint8_t key[cc::kResumeKeySize]{};
        uint64_t size = 0;
        uint64_t nextOffset = 0;
        uint64_t copied = 0;
        Request requests[cc::kRequestsInFlight];
        int pending = 0;       // requests in flight for this file
        bool aborted = false;  // stop; close handles and remove the partial destination
        bool closing = false;  // handles are being closed (finished or cleaning up)
        bool settled = false;
    };

    using Reply = std::function<void(int status, void* commandData)>;

    // Sends one request for `job`; once its last request has replied, the job settles.
    void send(Job& job, PipelinedOps::Issue issue, Reply onReply, bool cleanup = false) {
        ++job.pending;
        ops_.submit(std::move(issue),
                    [this, &job, onReply = std::move(onReply)](int status, void* data) {
                        --job.pending;
                        onReply(status, data);
                        if (job.pending == 0) settle(job);
                    },
                    cleanup);
    }

    // True when the job must stop: dropped request, a stopping runner, or an earlier abort.
    bool stopped(Job& job, int status) {
        if (status == PipelinedOps::kNotSent || ops_.stopping()) job.aborted = true;
        return job.aborted;
    }

    void abortErrno(Job& job, int status, const std::string& what) {
        job.aborted = true;
        ops_.failFromErrno(status, "Copy Failed: " + what + ": " + errorText());
    }

    void abortStatus(Job& job, uint32_t status, const std::string& what) {
        job.aborted = true;
        const std::string message = "Copy Failed: " + what + " for '" + job.file->from + "': " + cc::statusText(status);
        ops_.fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(0, static_cast<int>(status), message)), message);
    }

    void abortWith(Job& job, SmbErrorCode code, const std::string& message) {
        job.aborted = true;
        ops_.fail(code, message);
    }

    std::string errorText() const { return ops_.ctx() ? smb2_get_error(ops_.ctx()) : "connection lost"; }

    void startMore() {
        while (!ops_.stopping() && active_ < maxFiles_ && next_ < files_.size()) {
            jobs_.emplace_back();
            Job& job = jobs_.back();
            job.file = &files_[next_++];
            ++active_;
            begin(job);
        }
    }

    void begin(Job& job) {
        const std::string* from = &job.file->from;
        const std::string* to = &job.file->to;
        send(job, [from](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_open_async(ctx, from->c_str(), O_RDONLY, cb, data); },
             [this, &job](int status, void* data) {
                 if (status == 0 && data) job.source = static_cast<smb2fh*>(data);
                 if (stopped(job, status)) return;
                 if (status < 0) return abortErrno(job, status, "Could not open source '" + job.file->from + "'");
                 readSize(job);
             });
        send(job,
             [to](smb2_context* ctx, smb2_command_cb cb, void* data) {
                 return smb2_open_async(ctx, to->c_str(), O_WRONLY | O_CREAT | O_EXCL, cb, data);
             },
             [this, &job](int status, void* data) {
                 if (status == 0 && data) {
                     job.destination = static_cast<smb2fh*>(data);
                     job.destinationCreated = true;
                 }
                 if (stopped(job, status)) return;
                 if (status < 0) return abortErrno(job, status, "Could not create destination '" + job.file->to + "'");
                 startData(job);
             });
    }

    void readSize(Job& job) {
        send(job, [&job](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_fstat_async(ctx, job.source, &job.stat, cb, data); },
             [this, &job](int status, void*) {
                 if (stopped(job, status)) return;
                 if (status < 0) return abortErrno(job, status, "Could not stat source '" + job.file->from + "'");
                 job.statReady = true;
                 startData(job);
             });
    }

    // Runs once the source is open and sized and the destination exists.
    void startData(Job& job) {
        if (!job.source || !job.statReady || !job.destination) return;
        job.size = job.stat.smb2_size;
        if (job.size == 0) return finish(job);

        send(job,
             [&job](smb2_context* ctx, smb2_command_cb cb, void* data) {
                 return cc::queueFsctl(ctx, SMB2_FSCTL_SRV_REQUEST_RESUME_KEY, job.source, nullptr, cb, data) ? 0 : -ENOMEM;
             },
             [this, &job](int status, void* data) {
                 const auto s = static_cast<uint32_t>(status);
                 const bool keyReady = status != PipelinedOps::kNotSent && s == SMB2_STATUS_SUCCESS && ops_.ctx() &&
                                       cc::takeResumeKey(ops_.ctx(), data, job.key);
                 if (stopped(job, status)) return;
                 if (s != SMB2_STATUS_SUCCESS) return abortStatus(job, s, "resume key request failed");
                 if (!keyReady) return abortWith(job, SmbErrorCode::Io, "Copy Failed: invalid resume key reply");
                 fill(job);
             });
    }

    void fill(Job& job) {
        for (auto& request : job.requests) {
            if (job.aborted || ops_.stopping() || job.nextOffset >= job.size) return;
            if (request.busy) continue;

            const uint64_t length = cc::requestLength(job.size, job.nextOffset);
            cc::encodeRequest(job.key, job.nextOffset, length, request.input);
            request.busy = true;
            request.length = length;
            job.nextOffset += length;

            Request* req = &request;
            send(job,
                 [&job, req](smb2_context* ctx, smb2_command_cb cb, void* data) {
                     return cc::queueFsctl(ctx, SMB2_FSCTL_SRV_COPYCHUNK_WRITE, job.destination, &req->input, cb, data) ? 0 : -ENOMEM;
                 },
                 [this, &job, req](int status, void* data) {
                     req->busy = false;
                     const auto s = static_cast<uint32_t>(status);
                     const int64_t written =
                         status != PipelinedOps::kNotSent && s == SMB2_STATUS_SUCCESS && ops_.ctx() ? cc::takeBytesWritten(ops_.ctx(), data) : -1;
                     if (stopped(job, status)) return;
                     if (s != SMB2_STATUS_SUCCESS) return abortStatus(job, s, "server-side copy failed");
                     if (written != static_cast<int64_t>(req->length)) {
                         return abortWith(job, SmbErrorCode::Io, "Copy Failed: server copied " + std::to_string(written) + " of " +
                                                                     std::to_string(req->length) + " bytes for '" + job.file->from + "'");
                     }
                     job.copied += req->length;
                     if (totalCopied_) *totalCopied_ += static_cast<int64_t>(req->length);
                     progress_.emitProgress(totalCopied_ ? *totalCopied_ : 0, totalSize_);
                     if (job.copied == job.size)
                         finish(job);
                     else
                         fill(job);
                 });
        }
    }

    void finish(Job& job) {
        send(job, [&job](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_fsync_async(ctx, job.destination, cb, data); },
             [this, &job](int status, void*) {
                 if (stopped(job, status)) return;
                 if (status < 0) return abortErrno(job, status, "smb2_fsync '" + job.file->to + "'");
                 closeHandles(job, /*removeDestination=*/false);
             });
    }

    // Closes whatever is open; on cleanup also removes the partial destination.
    void closeHandles(Job& job, bool removeDestination) {
        job.closing = true;
        if (job.source) {
            send(job, [&job](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_close_async(ctx, job.source, cb, data); },
                 [&job](int, void*) { job.source = nullptr; }, /*cleanup=*/true);
        }
        if (job.destination) {
            send(job,
                 [&job](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_close_async(ctx, job.destination, cb, data); },
                 [this, &job, removeDestination](int status, void*) {
                     job.destination = nullptr;
                     if (removeDestination) return removePartial(job);
                     if (status < 0) abortErrno(job, status, "smb2_close '" + job.file->to + "'");
                 },
                 /*cleanup=*/true);
        } else if (removeDestination) {
            removePartial(job);
        }
    }

    void removePartial(Job& job) {
        if (!job.destinationCreated) return;
        const std::string* to = &job.file->to;
        send(job, [to](smb2_context* ctx, smb2_command_cb cb, void* data) { return smb2_unlink_async(ctx, to->c_str(), cb, data); },
             [](int, void*) {}, /*cleanup=*/true);
    }

    // Called once a job has no request in flight.
    void settle(Job& job) {
        if (job.settled) return;
        if (job.aborted && !job.closing) {
            closeHandles(job, /*removeDestination=*/true);
            if (job.pending > 0) return;
        }
        if (!job.closing && !job.aborted) return;  // still between steps (cannot happen while idle)
        job.settled = true;
        --active_;
        startMore();
    }

    PipelinedOps& ops_;
    const std::vector<FileCopyJob>& files_;
    size_t maxFiles_;
    std::shared_ptr<int64_t> totalCopied_;
    int64_t totalSize_;
    TransferCore progress_;

    std::deque<Job> jobs_;  // stable addresses
    size_t next_ = 0;
    size_t active_ = 0;
};

}  // namespace

void copyFilesServerSide(smb2_context* ctx, SmbConnectionManager& manager, const std::vector<FileCopyJob>& files, size_t maxFiles,
                         const std::shared_ptr<int64_t>& totalCopied, int64_t totalSize,
                         const std::function<void(double, double)>& onProgress, const CancellationToken& cancel) {
    if (files.empty()) return;
    // Each file has at most its open/stat/close requests plus its copy requests in flight.
    PipelinedOps ops(ctx, manager, cancel, maxFiles * (2 + cc::kRequestsInFlight), "Copy Failed");
    ConcurrentCopy copy(ops, files, maxFiles, totalCopied, totalSize, onProgress);
    copy.start();
    ops.run();
}

}  // namespace react_native_smb::detail
