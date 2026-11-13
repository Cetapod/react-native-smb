#pragma once

#include "CancellationToken.hpp"

namespace react_native_smb {

class SmbTask;
class SmbConnectionPool;
class PoolContextHandle;

// Everything an operator needs, injected by the owning task before run().
//   - task     : back-pointer to report status/progress
//   - pool     : connection pool; lifecycle operators drive its transitions
//   - cancel   : shared token the operator polls in its loop
//   - context  : slot granted before run(); null for lifecycle operators
struct OperatorContext {
    SmbTask* task{nullptr};                 // non-owning
    SmbConnectionPool* pool{nullptr};       // non-owning
    CancellationToken cancel;               // shared with the task
    PoolContextHandle* context{nullptr};    // non-owning
};

}  // namespace react_native_smb
