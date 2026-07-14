#pragma once

#include <cstddef>

#include "CancellationToken.hpp"

namespace react_native_smb {

class SmbTask;
class SmbConnectionPool;

// Everything an operator needs, injected by OperatorBase::start() before run().
//   - task     : back-pointer to report status/progress and spawn() more work
//   - pool     : connection pool; op calls requestContext() when it needs a slot
//   - cancel   : shared token the operator polls in its loop
//   - opIndex  : this operator's stable index for per-op aggregation
struct OperatorContext {
    SmbTask* task{nullptr};            // non-owning
    SmbConnectionPool* pool{nullptr};  // non-owning
    CancellationToken cancel;          // shared with the task
    size_t opIndex{0};
};

}  // namespace react_native_smb
