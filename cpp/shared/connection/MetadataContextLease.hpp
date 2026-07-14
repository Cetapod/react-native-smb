#pragma once

#include <string>
#include <utility>

#include "../core/SmbEnums.hpp"
#include "PoolContextHandle.hpp"
#include "PoolTypes.hpp"
#include "SmbConnectionPool.hpp"

namespace react_native_smb {

// Holds one metadata pool slot for multiple submitSync calls (e.g. entire copyTreeImpl).
class MetadataContextLease {
   public:
    MetadataContextLease(SmbConnectionPool& pool, SmbOperatorKind kind, const std::string& taskId)
        : handle_(pool.requestContext(AcquireMode::Metadata, kind, taskId)) {}

    MetadataContextLease(MetadataContextLease&&) = default;
    MetadataContextLease& operator=(MetadataContextLease&&) = default;
    MetadataContextLease(const MetadataContextLease&) = delete;
    MetadataContextLease& operator=(const MetadataContextLease&) = delete;

    template <typename Fn>
    auto submitSync(Fn&& fn) {
        return handle_.submitSync(std::forward<Fn>(fn));
    }

   private:
    PoolContextHandle handle_;
};

}  // namespace react_native_smb