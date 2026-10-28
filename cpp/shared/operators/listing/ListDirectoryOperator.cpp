#include "ListDirectoryOperator.hpp"

#include <any>
#include <chrono>
#include <memory>
#include <vector>

#include "../../ReactNativeSmb.hpp"  // SmbFileInfo + JSIConverter
#include "../../connection/SmbConnectionPool.hpp"
#include "../../core/EntryStream.hpp"
#include "../../io/listing/DirectoryLister.hpp"
#include "../../util/SmbLog.hpp"

namespace react_native_smb {

void ListDirectoryOperator::run() {
    std::shared_ptr<EntryStream> stream = entryStream();

    listing::DirectoryListOptions options;
    options.maxDepth = maxDepth_;
    options.includeSecurityDescriptor = includeSecurityDescriptor_;
    if (batchSize_ > 0) options.minBatch = static_cast<size_t>(batchSize_);

    const auto startedAt = std::chrono::steady_clock::now();
    auto handle = requestContext(maxDepth_ == 0 ? AcquireMode::Interactive : AcquireMode::Metadata);
    const listing::DirectoryLister::Stats stats = handle.submitSync([&](smb2_context* ctx) {
        listing::DirectoryLister lister(ctx, handle.manager(), path_, options, cancelToken(),
                                        [&stream](std::vector<SmbFileInfo>&& batch) { stream->append(std::move(batch)); });
        lister.run();
        return lister.stats();
    });
    if (isCancelled()) return;

    const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt).count();
    SMB_LOG_INFO("listDirectory(maxDepth=%d): %zu entries (%zu dirs, %zu listed dirs) in %zu batch(es); first batch %lld ms, total %lld ms",
                 maxDepth_, stats.entries, stats.directories, stats.listedDirectories, stats.batches,
                 static_cast<long long>(stats.firstBatchAfter ? stats.firstBatchAfter->count() : -1), static_cast<long long>(totalMs));

    // The stream already holds every entry; convert from it instead of keeping a copy.
    publishResult(std::any(), [stream](jsi::Runtime& rt, const std::any&) -> jsi::Value {
        return stream->read([&rt](const std::vector<SmbFileInfo>& entries) {
            return margelo::nitro::JSIConverter<std::vector<SmbFileInfo>>::toJSI(rt, entries);
        });
    });
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
