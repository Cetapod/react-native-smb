#include "PoolContextHandle.hpp"

#include "SmbConnectionPool.hpp"

namespace react_native_smb {

PoolContextHandle::PoolContextHandle(SmbConnectionPool* pool, size_t idx, SmbConnectionManager* conn)
    : pool_(pool), slotIndex_(idx), conn_(conn) {}

PoolContextHandle::~PoolContextHandle() {
    if (pool_ && conn_) pool_->releaseContext(*this);
}

PoolContextHandle::PoolContextHandle(PoolContextHandle&& o) noexcept : pool_(o.pool_), slotIndex_(o.slotIndex_), conn_(o.conn_) {
    o.pool_ = nullptr;
    o.conn_ = nullptr;
}

PoolContextHandle& PoolContextHandle::operator=(PoolContextHandle&& o) noexcept {
    if (this != &o) {
        if (pool_ && conn_) pool_->releaseContext(*this);
        pool_ = o.pool_;
        slotIndex_ = o.slotIndex_;
        conn_ = o.conn_;
        o.pool_ = nullptr;
        o.conn_ = nullptr;
    }
    return *this;
}

SmbConnectionManager& PoolContextHandle::manager() const { return *conn_; }

smb2_context* PoolContextHandle::ctx() const { return conn_->rawCtx(); }

}  // namespace react_native_smb