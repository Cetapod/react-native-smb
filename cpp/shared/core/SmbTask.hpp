#pragma once

#include <memory>
#include <optional>
#include <string>

#include "../connection/PoolTypes.hpp"
#include "../connection/SlotReservation.hpp"
#include "SmbTaskCore.hpp"

namespace react_native_smb {

class OperatorBase;
class SmbConnectionPool;
class EntryStream;

class SmbTask : public SmbTaskCore {
   public:
    SmbTask(std::string id, std::unique_ptr<OperatorBase> op, std::shared_ptr<SmbConnectionPool> pool);
    ~SmbTask();

    // Binds the operator and runs its prepare step. False when that settled the task.
    bool prepare();
    std::optional<AcquireMode> acquireMode() const;
    // Activates the reserved slot (none for lifecycle tasks), runs the operator
    // on the calling thread, and settles the task.
    void run(std::optional<SlotReservation> reservation);
    // Settles a task whose slot request was rejected.
    void reject(int code, const std::string& message);

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

    // Call from a catch block: settles from the in-flight exception.
    void settleFromCurrentException();
};

}  // namespace react_native_smb
