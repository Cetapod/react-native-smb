#include "HybridSmbTask.hpp"

#include <stdexcept>

#include "../core/SmbModels.hpp"
#include "../core/SmbTask.hpp"

namespace margelo::nitro::smb {

HybridSmbTask::HybridSmbTask(std::shared_ptr<react_native_smb::SmbTask> task)
    : HybridObject(TAG), task_(std::move(task)) {
    if (!task_) {
        throw std::invalid_argument("HybridSmbTask requires a non-null SmbTask");
    }
}

void HybridSmbTask::cancel() { task_->cancel(); }

std::unordered_map<std::string, std::string> HybridSmbTask::get() { return task_->get(); }

std::string HybridSmbTask::subscribe(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) {
    return task_->subscribeMap(listener);
}

void HybridSmbTask::unsubscribe(const std::string& subscriptionId) { task_->unsubscribe(subscriptionId); }

std::string HybridSmbTask::subscribeEntries(const std::function<void()>& listener) {
    return task_->subscribeEntries(listener);
}

void HybridSmbTask::unsubscribeEntries(const std::string& subscriptionId) { task_->unsubscribeEntries(subscriptionId); }

facebook::jsi::Value HybridSmbTask::getResultValueRaw(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& /*thisValue*/,
                                                      const facebook::jsi::Value* /*args*/, size_t /*count*/) {
    return task_->getResultValue(runtime);
}

facebook::jsi::Value HybridSmbTask::drainEntriesRaw(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& /*thisValue*/,
                                                    const facebook::jsi::Value* args, size_t count) {
    if (count < 1 || !args[0].isString()) {
        throw facebook::jsi::JSError(runtime, "drainEntries(subscriptionId: string) requires a string argument");
    }
    const std::string subscriptionId = args[0].asString(runtime).utf8(runtime);
    auto entries = task_->drainEntries(subscriptionId);
    return JSIConverter<std::vector<react_native_smb::SmbFileInfo>>::toJSI(runtime, entries);
}

void HybridSmbTask::loadHybridMethods() {
    HybridNativeSmbTaskSpec::loadHybridMethods();
    registerHybrids(this, [](Prototype& prototype) {
        prototype.registerRawHybridMethod("getResultValue", 0, &HybridSmbTask::getResultValueRaw);
        prototype.registerRawHybridMethod("drainEntries", 1, &HybridSmbTask::drainEntriesRaw);
    });
}

}  // namespace margelo::nitro::smb
