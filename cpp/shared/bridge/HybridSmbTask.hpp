#pragma once

#include "HybridNativeSmbTaskSpec.hpp"
#include "SmbTaskState.hpp"

#include <jsi/jsi.h>
#include <memory>
#include <string>

namespace react_native_smb {
class SmbTask;
}

namespace margelo::nitro::smb {

/**
 * Nitro adapter around domain SmbTask.
 * Typed Hybrid methods come from HybridNativeSmbTaskSpec.
 * getResultValue is registered as a raw JSI method.
 */
class HybridSmbTask : public virtual HybridNativeSmbTaskSpec {
   public:
    explicit HybridSmbTask(std::shared_ptr<react_native_smb::SmbTask> task);
    ~HybridSmbTask() override = default;

    void cancel() override;
    SmbTaskState get() override;
    std::string subscribe(const std::function<void(const SmbTaskState& /* snapshot */)>& listener) override;
    void unsubscribe(const std::string& subscriptionId) override;

    // Public so HybridSmbListTask can register the same raw method on its prototype.
    facebook::jsi::Value getResultValueRaw(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& thisValue,
                                           const facebook::jsi::Value* args, size_t count);

   protected:
    void loadHybridMethods() override;

    std::shared_ptr<react_native_smb::SmbTask> task_;
};

}  // namespace margelo::nitro::smb
