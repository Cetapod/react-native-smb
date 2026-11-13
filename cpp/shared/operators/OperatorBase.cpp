#include "OperatorBase.hpp"

#include "../connection/SmbConnectionPool.hpp"
#include "../core/SmbTask.hpp"
#include "../util/SmbException.hpp"

namespace react_native_smb {

void OperatorBase::attach(SmbTask* task, SmbConnectionPool* pool, const CancellationToken& cancel) {
    ctx_ = OperatorContext{task, pool, cancel};
}

void OperatorBase::start(PoolContextHandle* context) {
    ctx_.context = context;
    if (isCancelled()) return;
    run();
}

std::string OperatorBase::owningTaskId() const { return ctx_.task ? ctx_.task->getId() : std::string(); }

PoolContextHandle& OperatorBase::context() const {
    if (!ctx_.context) SmbException::raise(SmbErrorCode::NotConnected, "operator has no pool context");
    return *ctx_.context;
}

void OperatorBase::emitStatus(SmbTaskStatus s, const std::string& error, int code) {
    if (!ctx_.task) return;
    if (isCancelled() && s == SmbTaskStatus::Success) return;
    if (ctx_.task->getStatus() == SmbTaskStatus::Cancelled && s == SmbTaskStatus::Success) return;
    ctx_.task->onOpStatus(s, error, code);
}

void OperatorBase::emitProgress(double done, double total) {
    if (ctx_.task) ctx_.task->onOpProgress(done, total);
}

void OperatorBase::addExpectedBytes(int64_t bytes) {
    if (ctx_.task) ctx_.task->addExpectedBytes(bytes);
}

void OperatorBase::setSourceDestination(const std::string& source, const std::string& destination) {
    if (ctx_.task) ctx_.task->setPaths(source, destination);
}

void OperatorBase::publishResult(SmbTaskResult value) {
    if (ctx_.task) ctx_.task->publishResult(std::move(value));
}

std::shared_ptr<EntryStream> OperatorBase::entryStream() const {
    if (!ctx_.task) return nullptr;
    return ctx_.task->entryStream();
}

}  // namespace react_native_smb