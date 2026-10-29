#include "DirQuery.hpp"

#include <cstring>
#include <utility>

#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/smb2-errors.h>

namespace react_native_smb::listing {

namespace {
constexpr uint32_t kOutputBufferLength = 0xffff;

void fillQueryRequest(smb2_query_directory_request& req, const smb2_file_id fileId, const std::string& pattern) {
    std::memset(&req, 0, sizeof(req));
    req.file_information_class = SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION;
    std::memcpy(req.file_id, fileId, SMB2_FD_SIZE);
    req.output_buffer_length = kOutputBufferLength;
    req.name = pattern.c_str();
}
}  // namespace

DirQuery::DirQuery(smb2_context* ctx, std::string path, ChunkHandler onChunk, DoneHandler onDone, std::string pattern)
    : ctx_(ctx), path_(std::move(path)), onChunk_(std::move(onChunk)), onDone_(std::move(onDone)), pattern_(std::move(pattern)) {}

bool DirQuery::start() {
    smb2_create_request create;
    std::memset(&create, 0, sizeof(create));
    create.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
    create.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
    create.desired_access = SMB2_FILE_LIST_DIRECTORY | SMB2_FILE_READ_ATTRIBUTES;
    create.file_attributes = SMB2_FILE_ATTRIBUTE_DIRECTORY;
    create.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
    create.create_disposition = SMB2_FILE_OPEN;
    create.create_options = SMB2_FILE_DIRECTORY_FILE;
    create.name = path_.c_str();

    smb2_pdu* pdu = smb2_cmd_create_async(ctx_, &create, &DirQuery::onCreate, this);
    if (!pdu) return false;

    // Related compound: 0xFF… means "the handle the CREATE above opens".
    smb2_file_id related;
    std::memset(related, 0xFF, SMB2_FD_SIZE);
    smb2_query_directory_request query;
    fillQueryRequest(query, related, pattern_);
    smb2_pdu* next = smb2_cmd_query_directory_async(ctx_, &query, &DirQuery::onQuery, this);
    if (!next) {
        smb2_free_pdu(ctx_, pdu);
        return false;
    }
    smb2_add_compound_pdu(ctx_, pdu, next);
    smb2_queue_pdu(ctx_, pdu);
    outstanding_ = 2;
    return true;
}

void DirQuery::onCreate(smb2_context* /*smb2*/, int status, void* commandData, void* privateData) {
    auto* self = static_cast<DirQuery*>(privateData);
    --self->outstanding_;
    const auto s = static_cast<uint32_t>(status);
    if (s == SMB2_STATUS_SHUTDOWN) {
        self->finish(s);
        return;
    }
    if (s == SMB2_STATUS_SUCCESS && commandData) {
        std::memcpy(self->fileId_, static_cast<smb2_create_reply*>(commandData)->file_id, SMB2_FD_SIZE);
        self->opened_ = true;
    } else {
        self->createStatus_ = s == SMB2_STATUS_SUCCESS ? SMB2_STATUS_INVALID_NETWORK_RESPONSE : s;
    }
}

void DirQuery::onQuery(smb2_context* /*smb2*/, int status, void* commandData, void* privateData) {
    auto* self = static_cast<DirQuery*>(privateData);
    --self->outstanding_;
    self->handleQueryReply(static_cast<uint32_t>(status), commandData);
}

void DirQuery::onClose(smb2_context* /*smb2*/, int /*status*/, void* /*commandData*/, void* privateData) {
    --static_cast<DirQuery*>(privateData)->outstanding_;
}

void DirQuery::handleQueryReply(uint32_t status, void* commandData) {
    const bool firstQuery = firstQuery_;
    firstQuery_ = false;

    if (status == SMB2_STATUS_SHUTDOWN) {
        finish(status);
        return;
    }
    if (done_) return;
    if (createStatus_ != SMB2_STATUS_SUCCESS) {
        // The related QUERY_DIRECTORY inherits the CREATE failure; nothing is open.
        finish(createStatus_);
        return;
    }

    if (status == SMB2_STATUS_SUCCESS) {
        const auto* reply = static_cast<smb2_query_directory_reply*>(commandData);
        bool ok = reply != nullptr;
        if (ok && reply->output_buffer_length > 0) {
            try {
                ok = onChunk_(reply->output_buffer, reply->output_buffer_length);
            } catch (...) {
                ok = false;
            }
        }
        if (!ok) {
            finish(SMB2_STATUS_INVALID_NETWORK_RESPONSE);
        } else if (stopped_) {
            finish(SMB2_STATUS_CANCELLED);
        } else if (queueQuery()) {
            return;
        } else {
            finish(SMB2_STATUS_NO_MEMORY);
        }
    } else if (status == SMB2_STATUS_NO_MORE_FILES || (firstQuery && status == SMB2_STATUS_NO_SUCH_FILE)) {
        // Some servers answer the first query of an empty directory with NO_SUCH_FILE.
        finish(SMB2_STATUS_SUCCESS);
    } else {
        finish(status);
    }
    queueClose();
}

bool DirQuery::queueQuery() {
    smb2_query_directory_request query;
    fillQueryRequest(query, fileId_, pattern_);
    smb2_pdu* pdu = smb2_cmd_query_directory_async(ctx_, &query, &DirQuery::onQuery, this);
    if (!pdu) return false;
    smb2_queue_pdu(ctx_, pdu);
    ++outstanding_;
    return true;
}

void DirQuery::queueClose() {
    if (!opened_) return;
    opened_ = false;
    smb2_close_request close;
    std::memset(&close, 0, sizeof(close));
    std::memcpy(close.file_id, fileId_, SMB2_FD_SIZE);
    smb2_pdu* pdu = smb2_cmd_close_async(ctx_, &close, &DirQuery::onClose, this);
    if (!pdu) return;
    smb2_queue_pdu(ctx_, pdu);
    ++outstanding_;
}

void DirQuery::finish(uint32_t status) {
    if (done_) return;
    done_ = true;
    try {
        if (onDone_) onDone_(status);
    } catch (...) {
        // Never unwind through libsmb2's C callback stack.
    }
}

}  // namespace react_native_smb::listing
