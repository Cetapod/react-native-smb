#pragma once

#include <memory>
#include <string>

#include "SmbTaskCore.hpp"

namespace react_native_smb {

class OperatorBase;
class SmbConnectionPool;
class EntryStream;

class SmbTask : public SmbTaskCore {
   public:
    SmbTask(std::string id, std::unique_ptr<OperatorBase> op, std::shared_ptr<SmbConnectionPool> pool);
    ~SmbTask();

    // Runs the operator on the calling thread and settles the task.
    void run();

    using SmbTaskCore::getState;
    using SmbTaskCore::subscribe;
    using SmbTaskCore::unsubscribe;

    std::string getId() { return SmbTaskCore::getId(); }
    void cancel();

    const std::shared_ptr<EntryStream>& entryStream() const { return entries_; }

   private:
    std::unique_ptr<OperatorBase> op_;
    std::weak_ptr<SmbConnectionPool> pool_;
    std::shared_ptr<EntryStream> entries_;  // non-null only for ListDirectory
};

}  // namespace react_native_smb
