#include "DirectoryStreamer.hpp"

#include <poll.h>
#include <smb2/libsmb2.h>
#include <smb2/smb2-errors.h>

#include <algorithm>
#include <cerrno>
#include <utility>

#include "../../connection/SmbConnection.hpp"
#include "../../util/SmbException.hpp"
#include "../SmbPathUtil.hpp"
#include "DirEntryDecode.hpp"

namespace react_native_smb::listing {

namespace {
constexpr auto kPollInterval = std::chrono::milliseconds(100);
constexpr auto kIdleTimeout = std::chrono::seconds(60);
constexpr auto kDrainTimeout = std::chrono::seconds(2);

std::string statusText(uint32_t status) {
    const char* text = nterror_to_str(status);
    return text ? text : "unknown";
}

SmbErrorCode codeForStatus(uint32_t status, const std::string& message) {
    if (status == SMB2_STATUS_SHUTDOWN) return SmbErrorCode::NotConnected;
    return static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(0, static_cast<int>(status), message));
}
}  // namespace

DirectoryStreamer::DirectoryStreamer(smb2_context* ctx, SmbConnectionManager& manager, std::string path, DirectoryStreamOptions options,
                                     CancellationToken cancel, Sink sink)
    : ctx_(ctx),
      manager_(manager),
      path_(std::move(path)),
      options_(options),
      cancel_(std::move(cancel)),
      sink_(std::move(sink)),
      policy_(options.minBatch, options.maxDelay) {
    options_.maxConcurrentLookups = std::max<size_t>(1, options_.maxConcurrentLookups);
}

DirectoryStreamer::~DirectoryStreamer() {
    // Pending callbacks point into this object: tear the transport down first.
    if (ctx_ && !opsIdle()) invalidate();
}

// MARK: - Run loop

void DirectoryStreamer::run() {
    if (!ctx_) SmbException::raise(SmbErrorCode::NotConnected, "List Directory Failed: SMB context is not connected");

    startedAt_ = lastActivity_ = Clock::now();
    listing_ = std::make_unique<DirQuery>(
        ctx_, path_util::normalized(path_), [this](const uint8_t* buffer, uint32_t length) { return onListingChunk(buffer, length); },
        [this](uint32_t status) { onListingDone(status); });
    if (!listing_->start()) {
        SmbException::raiseFromSmb(ctx_, -1, "List Directory Failed: Could not queue directory query for '" + path_ + "'. Error: " +
                                                 smb2_get_error(ctx_));
    }

    for (;;) {
        const auto now = Clock::now();
        if (!stopping_ && cancel_.cancelled()) {
            cancelled_ = true;
            stopAll();
        }

        reapIdleOps();
        if (!stopping_) {
            launchLookups();
            flush(now);
        }
        if (finished()) break;

        if (stopping_ && drainDeadline_ && now > *drainDeadline_) {
            invalidate();
            break;
        }
        if (now - lastActivity_ > kIdleTimeout) {
            fail(SmbErrorCode::TimedOut, "List Directory Failed: directory query for '" + path_ + "' timed out");
            invalidate();
            break;
        }

        pollOnce(now);
        if (!ctx_) break;
    }

    if (failure_) throw SmbException(failure_->code, failure_->message);
}

void DirectoryStreamer::pollOnce(Clock::time_point now) {
    auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(kPollInterval);
    if (!stopping_) {
        if (const auto deadline = policy_.deadline(readyCount())) {
            timeout = std::clamp(std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now), std::chrono::milliseconds(0), timeout);
        }
    }

    pollfd pfd{};
    pfd.fd = smb2_get_fd(ctx_);
    pfd.events = static_cast<short>(smb2_which_events(ctx_));
    if (pfd.fd < 0) {
        fail(SmbErrorCode::NotConnected, "List Directory Failed: connection lost");
        invalidate();
        return;
    }

    const int result = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (result < 0) {
        if (errno == EINTR) return;
        fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromErrno(errno)), "List Directory Failed: poll failed");
        invalidate();
        return;
    }
    if (result == 0) return;

    lastActivity_ = Clock::now();
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        fail(SmbErrorCode::NotConnected, "List Directory Failed: socket closed");
        invalidate();
        return;
    }
    if (smb2_service(ctx_, pfd.revents) < 0) {
        const std::string message = std::string("List Directory Failed: ") + smb2_get_error(ctx_);
        fail(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(-1, smb2_get_nterror(ctx_), message, SmbErrorCode::NotConnected)),
             message);
        invalidate();
    }
}

bool DirectoryStreamer::finished() const {
    if (!opsIdle()) return false;
    return stopping_ || (listingComplete_ && lookups_.empty() && pending_.empty());
}

bool DirectoryStreamer::opsIdle() const {
    return (!listing_ || listing_->idle()) && countOps_.empty() && securityOps_.empty();
}

// MARK: - Listing

bool DirectoryStreamer::onListingChunk(const uint8_t* buffer, uint32_t length) {
    if (stopping_) return true;
    return forEachDirEntry(ctx_, buffer, length, [this](const smb2_fileidfulldirectoryinformation& info, const std::string& name) {
        Pending entry{path_util::makeFileInfo(statFromDirEntry(info), name, path_util::buildUrl(path_, name)), 0};
        const uint64_t seq = emitted_ + pending_.size();
        if (entry.info.isDirectory) {
            lookups_.push_back({LookupKind::ChildCount, seq});
            ++entry.outstanding;
            ++stats_.directories;
        }
        if (options_.includeSecurityDescriptor) {
            lookups_.push_back({LookupKind::SecurityDescriptor, seq});
            ++entry.outstanding;
        }
        pending_.push_back(std::move(entry));
        ++stats_.entries;
    });
}

void DirectoryStreamer::onListingDone(uint32_t status) {
    if (status == SMB2_STATUS_SUCCESS) {
        listingComplete_ = true;
        return;
    }
    if (stopping_) return;
    const std::string message = "List Directory Failed: Could not open directory '" + path_ + "'. Error: " + statusText(status);
    fail(codeForStatus(status, message), message);
}

// MARK: - Lookups

void DirectoryStreamer::launchLookups() {
    while (!lookups_.empty() && countOps_.size() + securityOps_.size() < options_.maxConcurrentLookups && !stopping_) {
        const Lookup lookup = lookups_.front();
        lookups_.pop_front();
        if (lookup.kind == LookupKind::ChildCount)
            startChildCount(lookup.seq);
        else
            startSecurityDescriptor(lookup.seq);
    }
}

void DirectoryStreamer::startChildCount(uint64_t seq) {
    Pending* entry = entryAt(seq);
    if (!entry) return;

    auto op = std::make_unique<ChildCountOp>();
    ChildCountOp* raw = op.get();
    raw->seq = seq;
    raw->query = std::make_unique<DirQuery>(
        ctx_, path_util::normalized(entry->info.path),
        [raw](const uint8_t* buffer, uint32_t length) {
            const int64_t count = countDirEntries(buffer, length);
            if (count < 0) return false;
            raw->count += count;
            return true;
        },
        [this, raw](uint32_t status) { onChildCountDone(*raw, status); });

    if (!raw->query->start()) {
        fail(SmbErrorCode::Io, "List Directory Failed: could not queue child count for '" + entry->info.path + "'");
        return;
    }
    countOps_.push_back(std::move(op));
}

void DirectoryStreamer::onChildCountDone(ChildCountOp& op, uint32_t status) {
    Pending* entry = entryAt(op.seq);
    if (!entry || stopping_) return;

    if (status == SMB2_STATUS_SUCCESS) {
        entry->info.childCount = op.count;
    } else {
        // Same policy as the synchronous scan: only transport failures abort the listing.
        const std::string message =
            "List Directory Failed: Could not count child directory '" + entry->info.path + "'. Error: " + statusText(status);
        const SmbErrorCode code = codeForStatus(status, message);
        if (code == SmbErrorCode::NotConnected || code == SmbErrorCode::TimedOut) {
            fail(code, message);
            return;
        }
        entry->info.childCount = -1;
    }
    completeLookup(op.seq);
}

void DirectoryStreamer::startSecurityDescriptor(uint64_t seq) {
    Pending* entry = entryAt(seq);
    if (!entry) return;

    auto op = std::make_unique<SecurityOp>();
    SecurityOp* raw = op.get();
    raw->seq = seq;
    raw->query.onFinished = [this, raw] { onSecurityDone(*raw); };

    if (queueSecurityDescriptorQuery(ctx_, path_util::normalized(entry->info.path), raw->query) != 0) {
        fail(SmbErrorCode::Io, "List Directory Failed: could not queue security descriptor query for '" + entry->info.path + "'");
        return;
    }
    securityOps_.push_back(std::move(op));
}

void DirectoryStreamer::onSecurityDone(SecurityOp& op) {
    Pending* entry = entryAt(op.seq);
    if (!entry || stopping_) return;

    if (op.query.failed()) {
        const SmbErrorCode code = op.query.errorCode();
        if (code != SmbErrorCode::AccessDenied) {
            fail(code, "List Directory Failed: security descriptor for '" + entry->info.path + "': " + op.query.error);
            return;
        }
        entry->info.securityDescriptor.reset();
    } else {
        entry->info.securityDescriptor = std::move(op.query.result);
    }
    completeLookup(op.seq);
}

void DirectoryStreamer::completeLookup(uint64_t seq) {
    if (Pending* entry = entryAt(seq); entry && entry->outstanding > 0) --entry->outstanding;
}

void DirectoryStreamer::reapIdleOps() {
    countOps_.remove_if([](const std::unique_ptr<ChildCountOp>& op) { return op->query->done() && op->query->idle(); });
    securityOps_.remove_if([](const std::unique_ptr<SecurityOp>& op) { return op->query.finished; });
}

// MARK: - Emission

size_t DirectoryStreamer::readyCount() {
    while (readyCursor_ < pending_.size() && pending_[readyCursor_].outstanding == 0) ++readyCursor_;
    return readyCursor_;
}

void DirectoryStreamer::flush(Clock::time_point now) {
    const size_t ready = readyCount();
    const bool complete = listingComplete_ && ready == pending_.size();
    if (!policy_.shouldFlush(ready, complete, now)) return;

    std::vector<SmbFileInfo> batch;
    batch.reserve(ready);
    for (size_t i = 0; i < ready; ++i) {
        batch.push_back(std::move(pending_.front().info));
        pending_.pop_front();
    }
    emitted_ += ready;
    readyCursor_ = 0;

    policy_.markFlushed(now);
    ++stats_.batches;
    if (!stats_.firstBatchAfter) stats_.firstBatchAfter = std::chrono::duration_cast<std::chrono::milliseconds>(now - startedAt_);
    sink_(std::move(batch));
}

// MARK: - Control

void DirectoryStreamer::fail(SmbErrorCode code, std::string message) {
    if (!failure_ && !cancelled_) failure_ = Failure{code, std::move(message)};
    stopAll();
}

void DirectoryStreamer::stopAll() {
    if (stopping_) return;
    stopping_ = true;
    drainDeadline_ = Clock::now() + kDrainTimeout;
    lookups_.clear();
    if (listing_) listing_->stop();
    for (auto& op : countOps_) op->query->stop();
}

void DirectoryStreamer::invalidate() {
    stopping_ = true;
    if (!ctx_) return;
    ctx_ = nullptr;
    // Destroys the context; libsmb2 fires every pending callback with SHUTDOWN.
    manager_.invalidateContext();
    countOps_.clear();
    securityOps_.clear();
}

DirectoryStreamer::Pending* DirectoryStreamer::entryAt(uint64_t seq) {
    if (seq < emitted_ || seq - emitted_ >= pending_.size()) return nullptr;
    return &pending_[static_cast<size_t>(seq - emitted_)];
}

}  // namespace react_native_smb::listing
