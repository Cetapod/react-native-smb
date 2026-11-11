#include "HybridSmbListTask.hpp"

#include <stdexcept>
#include <vector>

#include "DomainConverters.hpp"
#include "../core/EntryStream.hpp"
#include "../core/SmbModels.hpp"
#include "../core/SmbTask.hpp"

namespace margelo::nitro::smb {

HybridSmbListTask::HybridSmbListTask(std::shared_ptr<react_native_smb::SmbTask> task)
    : HybridObject(HybridNativeSmbListTaskSpec::TAG), HybridSmbTask(std::move(task)) {
    if (!task_->entryStream()) {
        throw std::invalid_argument("HybridSmbListTask requires a listing SmbTask with an EntryStream");
    }
}

std::string HybridSmbListTask::subscribeEntries(const std::function<void()>& listener) {
    return task_->entryStream()->subscribe(listener);
}

void HybridSmbListTask::unsubscribeEntries(const std::string& subscriptionId) {
    task_->entryStream()->unsubscribe(subscriptionId);
}

facebook::jsi::Value HybridSmbListTask::drainEntriesRaw(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& /*thisValue*/,
                                                        const facebook::jsi::Value* args, size_t count) {
    if (count < 1 || !args[0].isString()) {
        throw facebook::jsi::JSError(runtime, "drainEntries(subscriptionId: string) requires a string argument");
    }
    const std::string subscriptionId = args[0].asString(runtime).utf8(runtime);
    auto entries = task_->entryStream()->drain(subscriptionId);
    return JSIConverter<std::vector<react_native_smb::SmbFileInfo>>::toJSI(runtime, entries);
}

void HybridSmbListTask::loadHybridMethods() {
    HybridNativeSmbListTaskSpec::loadHybridMethods();
    registerHybrids(this, [](Prototype& prototype) {
        prototype.registerRawHybridMethod("getResultValue", 0, &HybridSmbTask::getResultValueRaw);
        prototype.registerRawHybridMethod("drainEntries", 1, &HybridSmbListTask::drainEntriesRaw);
    });
}

}  // namespace margelo::nitro::smb
