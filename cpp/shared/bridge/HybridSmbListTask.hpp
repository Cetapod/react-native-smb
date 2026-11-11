#pragma once

#include "HybridNativeSmbListTaskSpec.hpp"
#include "HybridSmbTask.hpp"

#include <jsi/jsi.h>
#include <memory>
#include <string>

namespace margelo::nitro::smb {

/**
 * Listing HybridObject: NativeSmbListTask methods plus raw drainEntries.
 * Virtual inheritance of HybridNativeSmbTaskSpec is required by Nitrogen.
 */
class HybridSmbListTask : public HybridNativeSmbListTaskSpec, public HybridSmbTask {
   public:
    explicit HybridSmbListTask(std::shared_ptr<react_native_smb::SmbTask> task);
    ~HybridSmbListTask() override = default;

    std::string subscribeEntries(const std::function<void()>& listener) override;
    void unsubscribeEntries(const std::string& subscriptionId) override;

   protected:
    void loadHybridMethods() override;

   private:
    facebook::jsi::Value drainEntriesRaw(facebook::jsi::Runtime& runtime, const facebook::jsi::Value& thisValue,
                                         const facebook::jsi::Value* args, size_t count);
};

}  // namespace margelo::nitro::smb
