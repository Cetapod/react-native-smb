#pragma once

#include "HybridNativeSmbTaskSpec.hpp"

#include <jsi/jsi.h>
#include <memory>
#include <string>
#include <unordered_map>

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
    std::unordered_map<std::string, std::string> get() override;
    std::string subscribe(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener) override;
    void unsubscribe(const std::string& subscriptionId) override;

   protected:
    void loadHybridMethods() override;

    std::shared_ptr<react_native_smb::SmbTask> task_;

    facebook::jsi::Value getResultValueRaw(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& thisValue,
                                           const facebook::jsi::Value* args, size_t count);
};


}  // namespace margelo::nitro::smb
