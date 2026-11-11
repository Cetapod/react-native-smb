#include "ListDirectoryOperator.hpp"

#include <memory>
#include <vector>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../core/EntryStream.hpp"
#include "../../core/SmbModels.hpp"
#include "../../core/SmbTaskResult.hpp"
#include "../../io/listing/DirectoryLister.hpp"

namespace react_native_smb {

void ListDirectoryOperator::run() {
    std::shared_ptr<EntryStream> stream = entryStream();

    listing::DirectoryListOptions options;
    options.maxDepth = maxDepth_;
    options.includeSecurityDescriptor = includeSecurityDescriptor_;
    if (batchSize_ > 0) options.minBatch = static_cast<size_t>(batchSize_);

    auto handle = requestContext(maxDepth_ == 0 ? AcquireMode::Interactive : AcquireMode::Metadata);
    handle.submitSync([&](smb2_context* ctx) {
        listing::DirectoryLister lister(ctx, handle.manager(), path_, options, cancelToken(),
                                        [&stream](std::vector<SmbFileInfo>&& batch) { stream->append(std::move(batch)); });
        lister.run();
    });
    if (isCancelled()) return;

    // Entries live in the stream; bridge reads them on getResultValue.
    publishResult(ListingResult{});
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
