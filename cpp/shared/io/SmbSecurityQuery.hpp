#pragma once

#include <functional>
#include <string>

#include "../ReactNativeSmb.hpp"
#include "../util/SmbErrorMapper.hpp"

struct smb2_context;

namespace react_native_smb {

class SmbConnectionManager;

// State of one in-flight CREATE + QUERY_INFO(security) + CLOSE compound.
// Caller-owned: it must outlive every libsmb2 callback of the compound, i.e.
// until `finished` or until the context is destroyed.
struct SecurityDescriptorQuery {
    SmbSecurityDescriptor result{0, {}, "", "", SmbAcl{0, 0, {}}};
    std::string error;
    bool finished{false};
    int createStatus{0};
    int queryStatus{0};
    // Optional; invoked once when the query finishes (on the servicing thread).
    std::function<void()> onFinished;

    bool failed() const { return !error.empty(); }
    SmbErrorCode errorCode() const;
};

// Queue the compound without waiting. Returns 0 when queued; callbacks then run
// from smb2_service() on the caller's poll loop.
int queueSecurityDescriptorQuery(smb2_context* ctx, const std::string& path, SecurityDescriptorQuery& query);

// Query owner, group, and DACL for one path using an already-acquired context.
SmbSecurityDescriptor querySecurityDescriptorOnCtx(smb2_context* ctx, const std::string& path, SmbConnectionManager& manager);

}  // namespace react_native_smb
