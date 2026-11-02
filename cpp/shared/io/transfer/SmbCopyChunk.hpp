#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/smb2-errors.h>

// Wire format for server-side copy (FSCTL_SRV_REQUEST_RESUME_KEY and
// FSCTL_SRV_COPYCHUNK_WRITE), shared by single-file and concurrent copies.
namespace react_native_smb::detail::copy_chunk {

// Within the protocol's default server limits (256 chunks, 1 MiB each, 16 MiB per request).
constexpr uint32_t kChunkLength = 1024 * 1024;
constexpr uint32_t kChunksPerRequest = 16;
constexpr uint64_t kRequestLength = static_cast<uint64_t>(kChunkLength) * kChunksPerRequest;
constexpr int kRequestsInFlight = 4;
constexpr size_t kResumeKeySize = 24;

inline bool isUnsupported(uint32_t status) {
    return status == SMB2_STATUS_NOT_SUPPORTED || status == SMB2_STATUS_INVALID_DEVICE_REQUEST ||
           status == SMB2_STATUS_NOT_IMPLEMENTED || status == SMB2_STATUS_INVALID_PARAMETER;
}

inline std::string statusText(uint32_t status) {
    const char* text = nterror_to_str(status);
    return text ? text : "unknown";
}

// Queues one FSCTL on `handle`. `input` must stay alive until the reply arrives.
inline smb2_pdu* queueFsctl(smb2_context* ctx, uint32_t ctlCode, smb2fh* handle, std::vector<uint8_t>* input, smb2_command_cb cb,
                            void* cbData) {
    smb2_ioctl_request request;
    std::memset(&request, 0, sizeof(request));
    request.ctl_code = ctlCode;
    std::memcpy(request.file_id, *smb2_get_file_id(handle), SMB2_FD_SIZE);
    request.flags = SMB2_0_IOCTL_IS_FSCTL;
    if (input) {
        request.input_count = static_cast<uint32_t>(input->size());
        request.input = input->data();
    }
    smb2_pdu* pdu = smb2_cmd_ioctl_async(ctx, &request, cb, cbData);
    if (pdu) smb2_queue_pdu(ctx, pdu);
    return pdu;
}

// Copies the resume key out of a RESUME_KEY reply and frees the reply data.
inline bool takeResumeKey(smb2_context* ctx, void* commandData, uint8_t (&key)[kResumeKeySize]) {
    auto* reply = static_cast<smb2_ioctl_reply*>(commandData);
    if (!reply || !reply->output) return false;
    const bool ok = reply->output_count >= kResumeKeySize;
    if (ok) std::memcpy(key, reply->output, kResumeKeySize);
    smb2_free_data(ctx, reply->output);
    return ok;
}

// Bytes the server reports as written in a COPYCHUNK reply (frees the reply data); -1 if malformed.
inline int64_t takeBytesWritten(smb2_context* ctx, void* commandData) {
    auto* reply = static_cast<smb2_ioctl_reply*>(commandData);
    if (!reply || !reply->output) return -1;
    int64_t written = -1;
    if (reply->output_count >= 12) {
        const auto* out = static_cast<const uint8_t*>(reply->output) + 8;  // TotalBytesWritten
        written = static_cast<int64_t>(static_cast<uint32_t>(out[0]) | (static_cast<uint32_t>(out[1]) << 8) |
                                       (static_cast<uint32_t>(out[2]) << 16) | (static_cast<uint32_t>(out[3]) << 24));
    }
    smb2_free_data(ctx, reply->output);
    return written;
}

// Length of the next request starting at `offset`.
inline uint64_t requestLength(uint64_t fileSize, uint64_t offset) { return std::min(kRequestLength, fileSize - offset); }

// SRV_COPYCHUNK_COPY for [offset, offset + length), same offsets on both sides.
inline void encodeRequest(const uint8_t (&key)[kResumeKeySize], uint64_t offset, uint64_t length, std::vector<uint8_t>& out) {
    const auto putU32 = [](uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
    };
    const auto putU64 = [](uint8_t* p, uint64_t v) {
        for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
    };
    constexpr size_t kHeaderSize = kResumeKeySize + 8;  // key, chunk count, reserved
    constexpr size_t kEntrySize = 24;                   // source offset, target offset, length, reserved

    const uint32_t chunks = static_cast<uint32_t>((length + kChunkLength - 1) / kChunkLength);
    out.assign(kHeaderSize + kEntrySize * chunks, 0);
    std::memcpy(out.data(), key, kResumeKeySize);
    putU32(out.data() + kResumeKeySize, chunks);
    for (uint32_t i = 0; i < chunks; ++i) {
        uint8_t* entry = out.data() + kHeaderSize + kEntrySize * i;
        const uint64_t chunkOffset = offset + static_cast<uint64_t>(i) * kChunkLength;
        putU64(entry, chunkOffset);
        putU64(entry + 8, chunkOffset);
        putU32(entry + 16, static_cast<uint32_t>(std::min<uint64_t>(kChunkLength, offset + length - chunkOffset)));
    }
}

}  // namespace react_native_smb::detail::copy_chunk
