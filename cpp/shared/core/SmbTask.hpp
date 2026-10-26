#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <NitroModules/HybridObject.hpp>
#include <jsi/jsi.h>

#include "SmbTaskCore.hpp"

namespace react_native_smb {

class OperatorBase;
class SmbConnectionPool;
class EntryStream;
struct SmbFileInfo;

namespace jsi = facebook::jsi;
using namespace margelo::nitro;

class SmbTask : public HybridObject, public SmbTaskCore {
   public:
    using EntriesListener = std::function<void(const std::vector<SmbFileInfo>&)>;

    SmbTask(std::string id, std::unique_ptr<OperatorBase> seedOp, std::shared_ptr<SmbConnectionPool> pool);
    ~SmbTask();

    void start();
    size_t spawn(std::vector<std::unique_ptr<OperatorBase>> moreOps);

    using SmbTaskCore::subscribe;

    std::string getId() { return SmbTaskCore::getId(); }
    void cancel();
    std::unordered_map<std::string, std::string> get();
    std::string subscribeMap(const std::function<void(const std::unordered_map<std::string, std::string>&)>& listener);
    void unsubscribe(const std::string& id) { SmbTaskCore::unsubscribe(id); }

    // Incremental listing entries (replays entries emitted before subscribing).
    std::string subscribeEntries(const EntriesListener& listener);
    void unsubscribeEntries(const std::string& id);
    const std::shared_ptr<EntryStream>& entryStream() const { return entries_; }

    std::future<void> result() { return completionFuture(); }

   private:
    jsi::Value getResultValueRaw(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
    std::weak_ptr<SmbConnectionPool> pool_;
    std::shared_ptr<EntryStream> entries_;

    void launchOp(size_t opIndex);
    size_t appendOps(std::vector<std::unique_ptr<OperatorBase>>& more);

    void loadHybridMethods() override;
};

}  // namespace react_native_smb
