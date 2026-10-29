#include "DirectoryLister.hpp"

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

DirectoryLister::DirectoryLister(smb2_context* ctx, SmbConnectionManager& manager, std::string path, DirectoryListOptions options,
                                 CancellationToken cancel, Sink sink)
    : ctx_(ctx),
      manager_(manager),
      path_(std::move(path)),
      options_(options),
      cancel_(std::move(cancel)),
      sink_(std::move(sink)),
      policy_(options.minBatch, options.maxDelay) {
    options_.maxConcurrentDirQueries = std::max<size_t>(1, options_.maxConcurrentDirQueries);
    options_.maxConcurrentSecurityQueries = std::max<size_t>(1, options_.maxConcurrentSecurityQueries);
}

DirectoryLister::~DirectoryLister() {
    // Pending callbacks point into this object: tear the transport down first.
    if (ctx_ && !opsIdle()) invalidate();
}

// MARK: - Run loop

void DirectoryLister::run() {
    if (!ctx_) SmbException::raise(SmbErrorCode::NotConnected, "List Directory Failed: SMB context is not connected");

    lastActivity_ = Clock::now();
    listing_ = std::make_unique<DirQuery>(
        ctx_, path_util::normalized(path_), [this](const uint8_t* buffer, uint32_t length) { return decodeEntries(nullptr, buffer, length); },
        [this](uint32_t status) { onListingDone(status); }, options_.pattern);
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

void DirectoryLister::pollOnce(Clock::time_point now) {
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

bool DirectoryLister::finished() const {
    if (!opsIdle()) return false;
    return stopping_ || (listingComplete_ && dirLookups_.empty() && securityLookups_.empty() && roots_.empty());
}

bool DirectoryLister::opsIdle() const {
    return (!listing_ || listing_->idle()) && dirOps_.empty() && securityOps_.empty();
}

// MARK: - Listing

bool DirectoryLister::decodeEntries(Node* parent, const uint8_t* buffer, uint32_t length) {
    if (stopping_) return true;

    const int level = parent ? parent->level + 1 : 0;
    const std::string& basePath = parent ? parent->info.path : path_;

    return forEachDirEntry(ctx_, buffer, length, [&](const smb2_fileidfulldirectoryinformation& info, const std::string& name) {
        auto node = std::make_unique<Node>();
        node->info = path_util::makeFileInfo(statFromDirEntry(info), name, path_util::buildUrl(basePath, name));
        node->parent = parent;
        node->topLevel = parent ? parent->topLevel : topLevelCount_++;
        node->level = level;
        if (node->info.isDirectory) {
            node->expanded = options_.maxDepth < 0 || level < options_.maxDepth;
            if (node->expanded || options_.countChildren) {
                dirLookups_.emplace(node->topLevel, Lookup{node->expanded ? LookupKind::Expand : LookupKind::ChildCount, node.get()});
                ++node->outstanding;
            }
        }
        if (options_.includeSecurityDescriptor) {
            securityLookups_.emplace(node->topLevel, Lookup{LookupKind::SecurityDescriptor, node.get()});
            ++node->outstanding;
        }

        if (!parent) {
            roots_.push_back(std::move(node));
            return;
        }
        if (node->outstanding > 0) ++parent->outstanding;
        parent->children.push_back(std::move(node));
    });
}

void DirectoryLister::onListingDone(uint32_t status) {
    if (status == SMB2_STATUS_SUCCESS) {
        listingComplete_ = true;
        return;
    }
    if (stopping_) return;
    const std::string message = "List Directory Failed: Could not open directory '" + path_ + "'. Error: " + statusText(status);
    fail(codeForStatus(status, message), message);
}

// MARK: - Lookups

// Lookups start in top-level order so earlier entries (with their subtrees)
// become ready first; directory queries go on the wire before security descriptors.
void DirectoryLister::launchLookups() {
    while (!dirLookups_.empty() && !stopping_ && dirOps_.size() < options_.maxConcurrentDirQueries) {
        const Lookup lookup = dirLookups_.begin()->second;
        dirLookups_.erase(dirLookups_.begin());
        startDirOp(lookup);
    }
    while (!securityLookups_.empty() && !stopping_ && securityOps_.size() < options_.maxConcurrentSecurityQueries) {
        const Lookup lookup = securityLookups_.begin()->second;
        securityLookups_.erase(securityLookups_.begin());
        startSecurityDescriptor(lookup.node);
    }
}

void DirectoryLister::startDirOp(const Lookup& lookup) {
    auto op = std::make_unique<DirOp>();
    DirOp* raw = op.get();
    raw->kind = lookup.kind;
    raw->node = lookup.node;

    DirQuery::ChunkHandler onChunk;
    if (lookup.kind == LookupKind::Expand) {
        onChunk = [this, raw](const uint8_t* buffer, uint32_t length) { return decodeEntries(raw->node, buffer, length); };
    } else {
        onChunk = [raw](const uint8_t* buffer, uint32_t length) {
            const int64_t count = countDirEntries(buffer, length);
            if (count < 0) return false;
            raw->count += count;
            return true;
        };
    }
    raw->query = std::make_unique<DirQuery>(ctx_, path_util::normalized(lookup.node->info.path), std::move(onChunk),
                                            [this, raw](uint32_t status) { onDirOpDone(*raw, status); });

    if (!raw->query->start()) {
        fail(SmbErrorCode::Io, "List Directory Failed: could not queue directory query for '" + lookup.node->info.path + "'");
        return;
    }
    dirOps_.push_back(std::move(op));
}

void DirectoryLister::onDirOpDone(DirOp& op, uint32_t status) {
    if (stopping_) return;
    Node* node = op.node;

    if (op.kind == LookupKind::Expand) {
        // Same as a nested listing before: any descendant failure fails the listing.
        if (status != SMB2_STATUS_SUCCESS) {
            const std::string message =
                "List Directory Failed: Could not open directory '" + node->info.path + "'. Error: " + statusText(status);
            fail(codeForStatus(status, message), message);
            return;
        }
    } else if (status == SMB2_STATUS_SUCCESS) {
        node->info.childCount = op.count;
    } else {
        // Only transport failures abort the listing; otherwise the count is unknown.
        const std::string message =
            "List Directory Failed: Could not count child directory '" + node->info.path + "'. Error: " + statusText(status);
        const SmbErrorCode code = codeForStatus(status, message);
        if (code == SmbErrorCode::NotConnected || code == SmbErrorCode::TimedOut) {
            fail(code, message);
            return;
        }
        node->info.childCount = -1;
    }
    release(node);
}

void DirectoryLister::startSecurityDescriptor(Node* node) {
    auto op = std::make_unique<SecurityOp>();
    SecurityOp* raw = op.get();
    raw->node = node;
    raw->query.onFinished = [this, raw] { onSecurityDone(*raw); };

    if (queueSecurityDescriptorQuery(ctx_, path_util::normalized(node->info.path), raw->query) != 0) {
        fail(SmbErrorCode::Io, "List Directory Failed: could not queue security descriptor query for '" + node->info.path + "'");
        return;
    }
    securityOps_.push_back(std::move(op));
}

void DirectoryLister::onSecurityDone(SecurityOp& op) {
    if (stopping_) return;
    Node* node = op.node;

    if (op.query.failed()) {
        const SmbErrorCode code = op.query.errorCode();
        if (code != SmbErrorCode::AccessDenied) {
            fail(code, "List Directory Failed: security descriptor for '" + node->info.path + "': " + op.query.error);
            return;
        }
        node->info.securityDescriptor.reset();
    } else {
        node->info.securityDescriptor = std::move(op.query.result);
    }
    release(node);
}

// Finishes one unit of a node's work. A finished expanded node folds its
// children into `children` (freeing their nodes) and releases its parent.
// Callers must not touch `node` afterwards: its parent may free it.
void DirectoryLister::release(Node* node) {
    if (node->outstanding == 0 || --node->outstanding > 0) return;

    if (node->expanded) {
        node->info.children.reserve(node->children.size());
        for (auto& child : node->children) node->info.children.push_back(std::move(child->info));
        node->info.childCount = static_cast<int64_t>(node->info.children.size());
        node->children.clear();
    }
    if (node->parent) release(node->parent);
}

void DirectoryLister::reapIdleOps() {
    dirOps_.remove_if([](const std::unique_ptr<DirOp>& op) { return op->query->done() && op->query->idle(); });
    securityOps_.remove_if([](const std::unique_ptr<SecurityOp>& op) { return op->query.finished; });
}

// MARK: - Emission

size_t DirectoryLister::readyCount() {
    while (readyCursor_ < roots_.size() && roots_[readyCursor_]->outstanding == 0) ++readyCursor_;
    return readyCursor_;
}

void DirectoryLister::flush(Clock::time_point now) {
    const size_t ready = readyCount();
    const bool complete = listingComplete_ && ready == roots_.size();
    if (!policy_.shouldFlush(ready, complete, now)) return;

    std::vector<SmbFileInfo> batch;
    batch.reserve(ready);
    for (size_t i = 0; i < ready; ++i) {
        batch.push_back(std::move(roots_.front()->info));
        roots_.pop_front();
    }
    readyCursor_ = 0;

    policy_.markFlushed(now);
    sink_(std::move(batch));
}

// MARK: - Control

void DirectoryLister::fail(SmbErrorCode code, std::string message) {
    if (!failure_ && !cancelled_) failure_ = Failure{code, std::move(message)};
    stopAll();
}

void DirectoryLister::stopAll() {
    if (stopping_) return;
    stopping_ = true;
    drainDeadline_ = Clock::now() + kDrainTimeout;
    dirLookups_.clear();
    securityLookups_.clear();
    if (listing_) listing_->stop();
    for (auto& op : dirOps_) op->query->stop();
}

void DirectoryLister::invalidate() {
    stopping_ = true;
    if (!ctx_) return;
    ctx_ = nullptr;
    // Destroys the context; libsmb2 fires every pending callback with SHUTDOWN.
    manager_.invalidateContext();
    dirOps_.clear();
    securityOps_.clear();
}

}  // namespace react_native_smb::listing
