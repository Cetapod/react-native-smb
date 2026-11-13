#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../connection/MetadataContextLease.hpp"
#include "../connection/PoolContextHandle.hpp"
#include "../connection/PoolTypes.hpp"
#include "../core/CancellationToken.hpp"
#include "../core/OperatorContext.hpp"
#include "../core/SmbEnums.hpp"
#include "../core/SmbTaskResult.hpp"

namespace react_native_smb {

class SmbConnectionPool;
class SmbTask;
class EntryStream;
struct SmbFileInfo;

class OperatorBase {
   public:
    virtual ~OperatorBase() = default;

    virtual void run() = 0;
    virtual SmbOperatorKind kind() const = 0;

    void setContext(const OperatorContext& ctx) { ctx_ = ctx; }
    void start(SmbTask* task, SmbConnectionPool* pool, const CancellationToken& cancel, size_t opIndex);

   protected:
    void emitStatus(SmbTaskStatus s, const std::string& error = "", int code = 0);
    void emitProgress(double done, double total);

    bool isCancelled() const { return ctx_.cancel.cancelled(); }
    const CancellationToken& cancelToken() const { return ctx_.cancel; }

    SmbConnectionPool& pool() const { return *ctx_.pool; }
    PoolContextHandle requestContext(AcquireMode mode);
    MetadataContextLease makeMetadataLease();
    std::string owningTaskId() const;

    size_t spawn(std::vector<std::unique_ptr<OperatorBase>> moreOps);
    void addExpectedBytes(int64_t bytes);
    void setSourceDestination(const std::string& source, const std::string& destination);
    void publishResult(SmbTaskResult value);
    // Owning task's incremental listing stream (null when detached from a task).
    std::shared_ptr<EntryStream> entryStream() const;

   private:
    OperatorContext ctx_{};
};

}  // namespace react_native_smb
