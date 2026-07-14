#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <jsi/jsi.h>

#include "../connection/MetadataContextLease.hpp"
#include "../connection/PoolContextHandle.hpp"
#include "../connection/PoolTypes.hpp"
#include "../core/CancellationToken.hpp"
#include "../core/OperatorContext.hpp"
#include "../core/SmbEnums.hpp"

namespace react_native_smb {

namespace jsi = facebook::jsi;

class SmbConnectionPool;
class SmbTask;

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
    void publishResult(std::any value, std::function<jsi::Value(jsi::Runtime&, const std::any&)> converter);

   private:
    OperatorContext ctx_{};
};

}  // namespace react_native_smb