#pragma once

#include "OperatorBase.hpp"

// For JSIConverter<T> in publishThisResult convenience.
#include "../ReactNativeSmb.hpp"  // brings the JSIConverter specializations in margelo::nitro namespace

namespace jsi = facebook::jsi;

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

    // Convenience for value-producing operators: publish our result_ + the
    // correct converter directly to the owning task. Call this at the end of a
    // successful run() (before or after emitStatus(Success)).
    void publishThisResult() {
        publishResult(std::any(result_), [](jsi::Runtime& rt, const std::any& a) -> jsi::Value {
            const T* v = std::any_cast<T>(&a);
            if (!v) return jsi::Value::undefined();
            return margelo::nitro::JSIConverter<T>::toJSI(rt, *v);
        });
    }
};

}  // namespace react_native_smb
