#include "GetSecurityDescriptorOperator.hpp"

#include <poll.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2.h>
#include <smb2/smb2.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "../../connection/SmbConnectionPool.hpp"
#include "../../io/SmbSecurity.hpp"

namespace react_native_smb {

namespace {

// Mutable state shared with the three compound-PDU callbacks.
struct AclQueryContext {
    SmbSecurityDescriptor* result;
    std::string error;
    bool finished;
    int createStatus;
    int queryStatus;
    int closeStatus;
    struct smb2_security_descriptor* sd;
    bool createCompleted;
    bool queryCompleted;
    bool closeCompleted;
};

int sendAclQuery(smb2_context* context, const std::string& filePath, AclQueryContext& queryContext) {
    auto createCallback = [](struct smb2_context* smb2, int status, void* /*command_data*/, void* private_data) {
        if (!private_data) return;
        auto* ctx = static_cast<AclQueryContext*>(private_data);
        ctx->createCompleted = true;
        ctx->createStatus = status;
        if (status != 0) {
            ctx->error = "CREATE failed (status=" + std::to_string(status) + "): " + (smb2 ? std::string(smb2_get_error(smb2)) : "");
        }
    };

    auto queryCallback = [](struct smb2_context* smb2, int status, void* command_data, void* private_data) {
        if (!private_data) return;
        auto* ctx = static_cast<AclQueryContext*>(private_data);
        ctx->queryCompleted = true;
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
        ctx->sd = static_cast<struct smb2_security_descriptor*>(rep->output_buffer);
        if (!ctx->sd) ctx->error = "QUERY_INFO: Invalid security descriptor format";
    };

    auto closeCallback = [](struct smb2_context* smb2, int status, void* /*command_data*/, void* private_data) {
        if (!private_data) return;
        auto* ctx = static_cast<AclQueryContext*>(private_data);
        ctx->closeCompleted = true;
        ctx->closeStatus = status;
        ctx->finished = true;  // final callback
        if (status != 0) {
            ctx->error = "CLOSE failed (status=" + std::to_string(status) + "): " + (smb2 ? std::string(smb2_get_error(smb2)) : "");
            return;
        }
        if (ctx->createStatus != 0) {
            ctx->error = "CREATE command failed with status: " + std::to_string(ctx->createStatus);
            return;
        }
        if (ctx->queryStatus != 0) {
            ctx->error = "QUERY_INFO command failed with status: " + std::to_string(ctx->queryStatus);
            return;
        }
        if (!ctx->sd) {
            ctx->error = "No security descriptor available for processing";
            return;
        }
        if (!ctx->result) {
            ctx->error = "Result pointer is null";
            return;
        }
        try {
            *ctx->result = SmbSecurity::processSecurityDescriptor(ctx->sd);
        } catch (const std::exception& e) {
            ctx->error = "Error converting security descriptor: " + std::string(e.what());
        } catch (...) {
            ctx->error = "Unknown error converting security descriptor";
        }
    };

    struct smb2_create_request cr_req;
    struct smb2_query_info_request qi_req;
    struct smb2_close_request cl_req;
    struct smb2_pdu *pdu, *next_pdu;

    memset(&cr_req, 0, sizeof(cr_req));
    cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
    cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
    cr_req.desired_access = SMB2_READ_CONTROL;
    cr_req.file_attributes = 0;
    cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE | SMB2_FILE_SHARE_DELETE;
    cr_req.create_disposition = SMB2_FILE_OPEN;
    cr_req.create_options = 0;
    cr_req.name = filePath.c_str();

    pdu = smb2_cmd_create_async(context, &cr_req, createCallback, &queryContext);
    if (!pdu) return -1;

    memset(&qi_req, 0, sizeof(qi_req));
    qi_req.info_type = SMB2_0_INFO_SECURITY;
    qi_req.output_buffer_length = 65535;
    qi_req.additional_information = SMB2_OWNER_SECURITY_INFORMATION | SMB2_GROUP_SECURITY_INFORMATION | SMB2_DACL_SECURITY_INFORMATION;
    memset(qi_req.file_id, 0xFF, SMB2_FD_SIZE);

    next_pdu = smb2_cmd_query_info_async(context, &qi_req, queryCallback, &queryContext);
    if (!next_pdu) {
        smb2_free_pdu(context, pdu);
        return -1;
    }
    smb2_add_compound_pdu(context, pdu, next_pdu);

    memset(&cl_req, 0, sizeof(cl_req));
    cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
    memset(cl_req.file_id, 0xFF, SMB2_FD_SIZE);

    next_pdu = smb2_cmd_close_async(context, &cl_req, closeCallback, &queryContext);
    if (!next_pdu) {
        smb2_free_pdu(context, pdu);
        return -1;
    }
    smb2_add_compound_pdu(context, pdu, next_pdu);

    smb2_queue_pdu(context, pdu);
    return 0;
}

void pollUntilComplete(smb2_context* ctx, AclQueryContext& context) {
    if (!ctx) throw std::runtime_error("ACL Error: SMB2 context is invalid during polling.");

    struct pollfd pfd;
    int timeout_count = 0;
    const int max_timeouts = 10;

    while (!context.finished) {
        pfd.fd = smb2_get_fd(ctx);
        if (pfd.fd < 0) throw std::runtime_error("ACL Error: Invalid file descriptor from libsmb2. Connection might be broken.");
        pfd.events = smb2_which_events(ctx);

        if (poll(&pfd, 1, 1000) < 0) throw std::runtime_error("Poll failed during ACL query");

        if (pfd.revents == 0) {
            if (++timeout_count >= max_timeouts) throw std::runtime_error("ACL query timed out");
            continue;
        }
        timeout_count = 0;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            throw std::runtime_error("ACL Error: Socket poll failure (revents=" + std::to_string(pfd.revents) + ").");
        }
        if (smb2_service(ctx, pfd.revents) < 0) {
            throw std::runtime_error("ACL Error: smb2_service failed: " + std::string(smb2_get_error(ctx)));
        }
    }
}

}  // namespace

void GetSecurityDescriptorOperator::run() {
    emitStatus(SmbTaskStatus::Running);

    auto handle = requestContext(AcquireMode::Interactive);

    SmbSecurityDescriptor sd{0, {}, "", "", SmbAcl{0, 0, {}}};

    handle.submitSync([&](smb2_context* ctx) {
        if (!ctx) throw std::runtime_error("ACL Operation Failed: SMB2 context is invalid. Please reconnect.");

        AclQueryContext queryContext = {&sd, "", false, 0, 0, 0, nullptr, false, false, false};

        if (sendAclQuery(ctx, path_, queryContext) != 0) {
            throw std::runtime_error("ACL Operation Failed: Could not send ACL query for '" + path_ + "'. Error: " + std::string(smb2_get_error(ctx)));
        }
        pollUntilComplete(ctx, queryContext);
        if (!queryContext.error.empty()) throw std::runtime_error(queryContext.error);
    });

    result_ = sd;
    publishThisResult();
    emitStatus(SmbTaskStatus::Success);
}

}  // namespace react_native_smb
