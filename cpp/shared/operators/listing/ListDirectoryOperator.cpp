#include "ListDirectoryOperator.hpp"

#include <any>
#include <memory>
#include <vector>

#include "../../ReactNativeSmb.hpp"  // SmbFileInfo + JSIConverter
#include "../../connection/SmbConnectionPool.hpp"
#include "../../core/EntryStream.hpp"
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

    // The stream already holds every entry; convert from it instead of keeping a copy.
    publishResult(std::any(), [stream](jsi::Runtime& rt, const std::any&) -> jsi::Value {
        return stream->read([&rt](const std::vector<SmbFileInfo>& entries) {
            return margelo::nitro::JSIConverter<std::vector<SmbFileInfo>>::toJSI(rt, entries);
        });
    });
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
