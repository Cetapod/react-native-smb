#include "SmbSecurityQuery.hpp"

#include <poll.h>

#include <cstddef>
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#include "SmbSecurity.hpp"
#include "../connection/SmbConnection.hpp"
#include "../util/SmbException.hpp"

namespace react_native_smb {

namespace {

struct AclQueryContext {
    SmbSecurityDescriptor result{0, {}, "", "", SmbAcl{0, 0, {}}};
    std::string error;
    bool finished{false};
    int createStatus{0};
    int queryStatus{0};
};

int sendAclQuery(smb2_context* context, const std::string& filePath, AclQueryContext& queryContext) {
    auto createCallback = [](struct smb2_context* smb2, int status, void* /*command_data*/, void* private_data) {
        if (!private_data) return;
        auto* ctx = static_cast<AclQueryContext*>(private_data);
        if (status == SMB2_STATUS_SHUTDOWN) {
            ctx->finished = true;
            ctx->error = "ACL query stopped because the SMB context shut down";
            return;
        }
        ctx->createStatus = status;
        if (status != 0) {
            ctx->error = "CREATE failed (status=" + std::to_string(status) + "): " + (smb2 ? std::string(smb2_get_error(smb2)) : "");
        }
    };

    auto queryCallback = [](struct smb2_context* smb2, int status, void* command_data, void* private_data) {
        if (!private_data) return;
        auto* ctx = static_cast<AclQueryContext*>(private_data);
        ctx->queryStatus = status;
        if (status != 0) {
            ctx->error = "QUERY_INFO failed (status=" + std::to_string(status) + "): " + (smb2 ? std::string(smb2_get_error(smb2)) : "");
            return;
        }
        if (!command_data) {
            ctx->error = "QUERY_INFO: No command data received";
            return;
        }
        auto rep = static_cast<struct smb2_query_info_reply*>(command_data);
        if (!rep || !rep->output_buffer) {
            ctx->error = "QUERY_INFO: No security descriptor data received";
            return;
        }
        auto* sd = static_cast<struct smb2_security_descriptor*>(rep->output_buffer);
        try {
            ctx->result = SmbSecurity::processSecurityDescriptor(sd);
        } catch (const std::exception& e) {
            ctx->error = "Error converting security descriptor: " + std::string(e.what());
        } catch (...) {
            ctx->error = "Unknown error converting security descriptor";
        }
        smb2_free_data(smb2, sd);
    };

    auto closeCallback = [](struct smb2_context* smb2, int status, void* /*command_data*/, void* private_data) {
        if (!private_data) return;
        auto* ctx = static_cast<AclQueryContext*>(private_data);
        ctx->finished = true;
        if (status != 0) {
            ctx->error = "CLOSE failed (status=" + std::to_string(status) + "): " + (smb2 ? std::string(smb2_get_error(smb2)) : "");
        } else if (ctx->createStatus != 0) {
            ctx->error = "CREATE command failed with status: " + std::to_string(ctx->createStatus);
        } else if (ctx->queryStatus != 0) {
            ctx->error = "QUERY_INFO command failed with status: " + std::to_string(ctx->queryStatus);
        }
    };

    struct smb2_create_request crReq;
    struct smb2_query_info_request qiReq;
    struct smb2_close_request clReq;
    struct smb2_pdu *pdu, *nextPdu;

    memset(&crReq, 0, sizeof(crReq));
    crReq.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
    crReq.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
    crReq.desired_access = SMB2_READ_CONTROL;
    crReq.file_attributes = 0;
    crReq.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE | SMB2_FILE_SHARE_DELETE;
    crReq.create_disposition = SMB2_FILE_OPEN;
    crReq.create_options = 0;
    crReq.name = filePath.c_str();

    pdu = smb2_cmd_create_async(context, &crReq, createCallback, &queryContext);
    if (!pdu) return -1;

    memset(&qiReq, 0, sizeof(qiReq));
    qiReq.info_type = SMB2_0_INFO_SECURITY;
    qiReq.output_buffer_length = 65535;
    qiReq.additional_information = SMB2_OWNER_SECURITY_INFORMATION | SMB2_GROUP_SECURITY_INFORMATION | SMB2_DACL_SECURITY_INFORMATION;
    memset(qiReq.file_id, 0xFF, SMB2_FD_SIZE);

    nextPdu = smb2_cmd_query_info_async(context, &qiReq, queryCallback, &queryContext);
    if (!nextPdu) {
        smb2_free_pdu(context, pdu);
        return -1;
    }
    smb2_add_compound_pdu(context, pdu, nextPdu);

    memset(&clReq, 0, sizeof(clReq));
    clReq.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
    memset(clReq.file_id, 0xFF, SMB2_FD_SIZE);

    nextPdu = smb2_cmd_close_async(context, &clReq, closeCallback, &queryContext);
    if (!nextPdu) {
        smb2_free_pdu(context, pdu);
        return -1;
    }
    smb2_add_compound_pdu(context, pdu, nextPdu);

    smb2_queue_pdu(context, pdu);
    return 0;
}

void pollUntilComplete(smb2_context* ctx, AclQueryContext& context) {
    struct pollfd pfd;
    int timeoutCount = 0;
    constexpr int kMaxTimeouts = 10;

    while (!context.finished) {
        pfd.fd = smb2_get_fd(ctx);
        if (pfd.fd < 0) SmbException::raise(SmbErrorCode::NotConnected, "ACL Error: invalid file descriptor from libsmb2");
        pfd.events = smb2_which_events(ctx);
        pfd.revents = 0;

        if (poll(&pfd, 1, 1000) < 0) {
            if (errno == EINTR) continue;
            SmbException::raiseFromErrno(errno, "ACL Error: poll failed");
        }

        if (pfd.revents == 0) {
            if (++timeoutCount >= kMaxTimeouts) SmbException::raise(SmbErrorCode::TimedOut, "ACL query timed out");
            continue;
        }
        timeoutCount = 0;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            SmbException::raise(SmbErrorCode::NotConnected, "ACL Error: socket poll failure (revents=" + std::to_string(pfd.revents) + ")");
        }
        if (smb2_service(ctx, pfd.revents) < 0) {
            SmbException::raiseFromSmb(ctx, -1, "ACL Error: smb2_service failed: " + std::string(smb2_get_error(ctx)), SmbErrorCode::NotConnected);
        }
    }
}

}  // namespace

SmbSecurityDescriptor querySecurityDescriptorOnCtx(smb2_context* ctx, const std::string& path, SmbConnectionManager& manager) {
    if (!ctx) SmbException::raise(SmbErrorCode::NotConnected, "ACL Operation Failed: SMB2 context is invalid. Please reconnect.");

    auto queryContext = std::make_unique<AclQueryContext>();

    if (sendAclQuery(ctx, path, *queryContext) != 0) {
        SmbException::raiseFromSmb(ctx, -1, "ACL Operation Failed: Could not send ACL query for '" + path + "'. Error: " + std::string(smb2_get_error(ctx)));
    }
    try {
        pollUntilComplete(ctx, *queryContext);
    } catch (...) {
        manager.invalidateContext();
        throw;
    }
    if (!queryContext->error.empty()) {
        const int status = queryContext->createStatus != 0 ? queryContext->createStatus : queryContext->queryStatus;
        throw SmbException(static_cast<SmbErrorCode>(SmbErrorMapper::fromSmbFailure(
                               0, status, queryContext->error)),
                           queryContext->error);
    }

    return queryContext->result;
}

}  // namespace react_native_smb
