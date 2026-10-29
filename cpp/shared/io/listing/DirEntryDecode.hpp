#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

namespace react_native_smb::listing {

// Walks a FILE_ID_FULL_DIRECTORY_INFORMATION buffer from QUERY_DIRECTORY.
// Calls fn(const smb2_fileidfulldirectoryinformation&, const std::string& name) for every entry except
// "." and "..". Returns false if the buffer is malformed.
template <typename Fn>
bool forEachDirEntry(smb2_context* ctx, const uint8_t* buffer, uint32_t length, Fn&& fn) {
    uint32_t offset = 0;
    for (;;) {
        if (offset >= length || length - offset < SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE) return false;

        smb2_iovec vec{const_cast<uint8_t*>(buffer + offset), length - offset, nullptr};
        smb2_fileidfulldirectoryinformation info{};
        if (smb2_decode_fileidfulldirectoryinformation(ctx, &info, &vec) < 0) return false;
        if (!info.name) return false;

        const std::string name(info.name);
        free(const_cast<char*>(info.name));
        if (name != "." && name != "..") fn(info, name);

        if (info.next_entry_offset == 0) return true;
        offset += info.next_entry_offset;
    }
}

// Counts entries (excluding "." and "..") without decoding names to UTF-8.
// Returns -1 if the buffer is malformed.
inline int64_t countDirEntries(const uint8_t* buffer, uint32_t length) {
    constexpr uint32_t kNameLengthOffset = 60;
    const auto u32 = [buffer](uint32_t at) {
        return static_cast<uint32_t>(buffer[at]) | (static_cast<uint32_t>(buffer[at + 1]) << 8) |
               (static_cast<uint32_t>(buffer[at + 2]) << 16) | (static_cast<uint32_t>(buffer[at + 3]) << 24);
    };
    const auto isDot = [buffer](uint32_t at) { return buffer[at] == '.' && buffer[at + 1] == 0; };

    int64_t count = 0;
    uint32_t offset = 0;
    for (;;) {
        if (offset >= length || length - offset < SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE) return -1;
        const uint32_t nameLength = u32(offset + kNameLengthOffset);
        const uint32_t nameAt = offset + SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE;
        if (nameLength > length - nameAt) return -1;

        const bool dot = nameLength == 2 && isDot(nameAt);
        const bool dotDot = nameLength == 4 && isDot(nameAt) && isDot(nameAt + 2);
        if (!dot && !dotDot) ++count;

        const uint32_t next = u32(offset);
        if (next == 0) return count;
        offset += next;
    }
}

// Same type mapping as libsmb2's readdir (reparse points become links).
inline smb2_stat_64 statFromDirEntry(const smb2_fileidfulldirectoryinformation& info) {
    smb2_stat_64 st{};
    st.smb2_type = SMB2_TYPE_FILE;
    if (info.file_attributes & SMB2_FILE_ATTRIBUTE_DIRECTORY) st.smb2_type = SMB2_TYPE_DIRECTORY;
    if (info.file_attributes & SMB2_FILE_ATTRIBUTE_REPARSE_POINT) st.smb2_type = SMB2_TYPE_LINK;
    st.smb2_ino = info.file_id;
    st.smb2_size = info.end_of_file;
    st.smb2_atime = info.last_access_time.tv_sec;
    st.smb2_atime_nsec = info.last_access_time.tv_usec * 1000;
    st.smb2_mtime = info.last_write_time.tv_sec;
    st.smb2_mtime_nsec = info.last_write_time.tv_usec * 1000;
    st.smb2_ctime = info.change_time.tv_sec;
    st.smb2_ctime_nsec = info.change_time.tv_usec * 1000;
    st.smb2_btime = info.creation_time.tv_sec;
    st.smb2_btime_nsec = info.creation_time.tv_usec * 1000;
    return st;
}

}  // namespace react_native_smb::listing
