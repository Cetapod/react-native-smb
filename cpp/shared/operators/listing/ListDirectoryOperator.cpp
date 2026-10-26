#include "ListDirectoryOperator.hpp"

#include <chrono>
#include <memory>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../core/EntryStream.hpp"
#include "../../io/SmbParallelDirScan.hpp"
#include "../../io/listing/DirectoryStreamer.hpp"
#include "../../util/SmbLog.hpp"

namespace react_native_smb {

namespace {
constexpr int kDefaultBatchSize = 10;
}  // namespace

void ListDirectoryOperator::run() {
    if (recursive_ && maxDepth_ != 0)
        runRecursive();
    else
        runStreaming();
}

void ListDirectoryOperator::runRecursive() {
    result_ = listDirectoryParallel(pool(), owningTaskId(), path_, maxDepth_, cancelToken(), includeSecurityDescriptor_);
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

void ListDirectoryOperator::runStreaming() {
    std::shared_ptr<EntryStream> stream = entryStream();
    if (!stream) stream = std::make_shared<EntryStream>();

    listing::DirectoryStreamOptions options;
    options.minBatch = static_cast<size_t>(batchSize_ > 0 ? batchSize_ : kDefaultBatchSize);
    options.includeSecurityDescriptor = includeSecurityDescriptor_;

    const auto startedAt = std::chrono::steady_clock::now();
    auto handle = requestContext(AcquireMode::Interactive);
    const listing::DirectoryStreamer::Stats stats = handle.submitSync([&](smb2_context* ctx) {
        listing::DirectoryStreamer streamer(ctx, handle.manager(), path_, options, cancelToken(),
                                            [&stream](std::vector<SmbFileInfo>&& batch) { stream->append(std::move(batch)); });
        streamer.run();
        return streamer.stats();
    });
    if (isCancelled()) return;

    const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt).count();
    SMB_LOG_INFO("listDirectory: %zu entries (%zu dirs) in %zu batch(es); first batch %lld ms, total %lld ms", stats.entries,
                 stats.directories, stats.batches, static_cast<long long>(stats.firstBatchAfter ? stats.firstBatchAfter->count() : -1),
                 static_cast<long long>(totalMs));

    // The stream already holds every entry; convert from it instead of keeping a copy.
    publishResult(std::any(), [stream](jsi::Runtime& rt, const std::any&) -> jsi::Value {
        return stream->read([&rt](const std::vector<SmbFileInfo>& entries) {
            return margelo::nitro::JSIConverter<std::vector<SmbFileInfo>>::toJSI(rt, entries);
        });
    });
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
