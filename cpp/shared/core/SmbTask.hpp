#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "SmbTaskCore.hpp"

namespace react_native_smb {

class OperatorBase;
class SmbConnectionPool;
class EntryStream;
struct SmbFileInfo;

class SmbTask : public SmbTaskCore, public std::enable_shared_from_this<SmbTask> {
   public:
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

    const std::shared_ptr<EntryStream>& entryStream() const { return entries_; }

    std::future<void> result() { return completionFuture(); }

   private:
    std::weak_ptr<SmbConnectionPool> pool_;
    std::shared_ptr<EntryStream> entries_;  // non-null only for ListDirectory

    void launchOp(size_t opIndex);
    size_t appendOps(std::vector<std::unique_ptr<OperatorBase>>& more);
};

}  // namespace react_native_smb
