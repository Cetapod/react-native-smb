#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

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

    virtual SmbOperatorKind kind() const = 0;
    // Slot the task acquires before run(); nullopt for lifecycle operators, which run without one.
    virtual std::optional<AcquireMode> acquireMode() const = 0;
    // Validates arguments and records task paths before the task waits for a slot. No SMB I/O.
    virtual void prepare() {}
    virtual void run() = 0;

    void attach(SmbTask* task, SmbConnectionPool* pool, const CancellationToken& cancel);
    // Runs the operator on the granted slot (null for lifecycle operators).
    void start(PoolContextHandle* context);

   protected:
    void emitStatus(SmbTaskStatus s, const std::string& error = "", int code = 0);
    void emitProgress(double done, double total);

    bool isCancelled() const { return ctx_.cancel.cancelled(); }
    const CancellationToken& cancelToken() const { return ctx_.cancel; }

    SmbConnectionPool& pool() const { return *ctx_.pool; }
    PoolContextHandle& context() const;
    std::string owningTaskId() const;

    void addExpectedBytes(int64_t bytes);
    void setSourceDestination(const std::string& source, const std::string& destination);
    void publishResult(SmbTaskResult value);
    // Owning task's incremental listing stream (null when detached from a task).
    std::shared_ptr<EntryStream> entryStream() const;

   private:
    OperatorContext ctx_{};
};

}  // namespace react_native_smb
