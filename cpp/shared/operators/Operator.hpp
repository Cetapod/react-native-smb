#pragma once

#include "OperatorBase.hpp"

#include "../core/SmbTaskResult.hpp"

namespace react_native_smb {

// Operators that produce a value store it here; the concrete HybridSMB method
// reads it back via result() once the task completes (typed, no erasure).
// void-producing operators derive from OperatorBase directly.
template <typename T>
class Operator : public OperatorBase {
   public:
    const T& result() const { return result_; }

   protected:
    T result_{};

    // Convenience for value-producing operators: publish our result_ directly
    // to the owning task. Call this at the end of a successful run().
    void publishThisResult() { publishResult(SmbTaskResult{result_}); }
};

}  // namespace react_native_smb
