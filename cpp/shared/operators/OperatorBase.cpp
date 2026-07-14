#include "OperatorBase.hpp"

#include "../connection/SmbConnectionPool.hpp"
#include "../core/SmbTask.hpp"

namespace react_native_smb {

void OperatorBase::start(SmbTask* task, SmbConnectionPool* pool, const CancellationToken& cancel, size_t opIndex) {
    setContext(OperatorContext{task, pool, cancel, opIndex});
    if (cancel.cancelled()) return;
    run();
}

std::string OperatorBase::owningTaskId() const { return ctx_.task ? ctx_.task->getId() : std::string(); }

MetadataContextLease OperatorBase::makeMetadataLease() {
    if (ctx_.task) ctx_.task->markPending();
    MetadataContextLease lease(pool(), kind(), owningTaskId());
    if (ctx_.task && !isCancelled()) ctx_.task->markRunning();
    return lease;
}

PoolContextHandle OperatorBase::requestContext(AcquireMode mode) {
    if (ctx_.task) ctx_.task->markPending();
    PoolContextHandle handle = pool().requestContext(mode, kind(), owningTaskId());
    if (ctx_.task && !isCancelled()) ctx_.task->markRunning();
    return handle;
}

void OperatorBase::emitStatus(SmbTaskStatus s, const std::string& error, int code) {
    if (!ctx_.task) return;
    if (isCancelled() && s == SmbTaskStatus::Success) return;
    if (ctx_.task->getStatus() == SmbTaskStatus::Cancelled && s == SmbTaskStatus::Success) return;
    ctx_.task->onOpStatus(ctx_.opIndex, s, error, code);
}

void OperatorBase::emitProgress(double done, double total) {
    if (ctx_.task) ctx_.task->onOpProgress(ctx_.opIndex, done, total);
}

size_t OperatorBase::spawn(std::vector<std::unique_ptr<OperatorBase>> moreOps) {
    return ctx_.task ? ctx_.task->spawn(std::move(moreOps)) : 0;
}

void OperatorBase::addExpectedBytes(int64_t bytes) {
    if (ctx_.task) ctx_.task->addExpectedBytes(bytes);
}

void OperatorBase::setSourceDestination(const std::string& source, const std::string& destination) {
    if (ctx_.task) ctx_.task->setPaths(source, destination);
}

void OperatorBase::publishResult(std::any value, std::function<jsi::Value(jsi::Runtime&, const std::any&)> converter) {
    if (ctx_.task) ctx_.task->publishResult(std::move(value), std::move(converter));
}

}  // namespace react_native_smb