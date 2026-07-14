#pragma once

#include <cstddef>
#include <ctime>
#include <type_traits>
#include <utility>

#include <smb2/smb2.h>

#include "SmbConnection.hpp"

namespace react_native_smb {

class SmbConnectionPool;

class PoolContextHandle {
   public:
    PoolContextHandle() = default;
    PoolContextHandle(SmbConnectionPool* pool, size_t idx, SmbConnectionManager* conn);
    ~PoolContextHandle();

    PoolContextHandle(PoolContextHandle&& o) noexcept;
    PoolContextHandle& operator=(PoolContextHandle&& o) noexcept;
    PoolContextHandle(const PoolContextHandle&) = delete;
    PoolContextHandle& operator=(const PoolContextHandle&) = delete;

    bool valid() const { return conn_ != nullptr; }
    size_t slotIndex() const { return slotIndex_; }

    SmbConnectionManager& manager() const;
    smb2_context* ctx() const;

    template <typename Fn, typename R = std::invoke_result_t<Fn, smb2_context*>>
    R submitSync(Fn&& fn) {
        return conn_->submitSync(std::forward<Fn>(fn));
    }

   private:
    SmbConnectionPool* pool_{nullptr};
    size_t slotIndex_{0};
    SmbConnectionManager* conn_{nullptr};
};

}  // namespace react_native_smb