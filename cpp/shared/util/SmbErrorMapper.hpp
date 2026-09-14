#pragma once

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

namespace react_native_smb {

enum class SmbErrorCode : int {
    Unknown = 0,
    Cancelled = 1,

    InvalidArgument = 100,
    NotFound = 101,
    AlreadyExists = 102,
    AccessDenied = 103,
    NotDirectory = 104,
    IsDirectory = 105,
    DirectoryNotEmpty = 106,

    NotConnected = 200,
    ConnectionRefused = 201,
    TimedOut = 202,
    AuthenticationFailed = 203,

    Busy = 300,
    NoSpace = 301,
    Io = 302,
};

class SmbErrorMapper {
   public:
    static int normalizeErrnoCode(int resultCode) {
        if (resultCode == 0) {
            return 0;
        }
        return resultCode < 0 ? -resultCode : resultCode;
    }

    static int fromErrnoResult(int resultCode) { return fromErrno(normalizeErrnoCode(resultCode)); }

    static int fromErrnoOrMessage(int errnoCode, const std::string& errorMessage) {
        const int mappedFromErrno = fromErrno(errnoCode);
        if (mappedFromErrno != static_cast<int>(SmbErrorCode::Unknown)) {
            return mappedFromErrno;
        }

        return fromMessage(errorMessage);
    }

    // libsmb2 results are usually negative errno values, but some APIs return
    // a generic -1. Prefer the SMB status and diagnostic before treating it as errno.
    static int fromSmbFailure(int resultCode, int ntStatus, const std::string& errorMessage,
                              SmbErrorCode fallback = SmbErrorCode::Io) {
        const int mappedFromStatus = fromNtStatus(static_cast<uint32_t>(ntStatus));
        if (mappedFromStatus != static_cast<int>(SmbErrorCode::Unknown)) return mappedFromStatus;

        const int mappedFromMessage = fromMessage(errorMessage);
        if (mappedFromMessage != static_cast<int>(SmbErrorCode::Unknown)) return mappedFromMessage;

        // -1 is a generic failure from smb2_service(), not -EPERM.
        if (resultCode != -1 && resultCode != 0) {
            const int mappedFromErrno = fromErrnoResult(resultCode);
            if (mappedFromErrno != static_cast<int>(SmbErrorCode::Unknown)) return mappedFromErrno;
        }
        return static_cast<int>(fallback);
    }

    static int fromErrno(int errnoCode) {
        switch (errnoCode) {
            case 0:
                return static_cast<int>(SmbErrorCode::Unknown);
            case ECANCELED:
                return static_cast<int>(SmbErrorCode::Cancelled);
            case EINVAL:
                return static_cast<int>(SmbErrorCode::InvalidArgument);
            case ENOENT:
                return static_cast<int>(SmbErrorCode::NotFound);
            case EEXIST:
                return static_cast<int>(SmbErrorCode::AlreadyExists);
            case EACCES:
            case EPERM:
                return static_cast<int>(SmbErrorCode::AccessDenied);
            case ENOTDIR:
                return static_cast<int>(SmbErrorCode::NotDirectory);
            case EISDIR:
                return static_cast<int>(SmbErrorCode::IsDirectory);
            case ENOTEMPTY:
                return static_cast<int>(SmbErrorCode::DirectoryNotEmpty);
            case ENOTCONN:
            case EHOSTUNREACH:
            case EPIPE:
#ifdef ECONNRESET
            case ECONNRESET:
#endif
#ifdef ECONNABORTED
            case ECONNABORTED:
#endif
#ifdef ENETUNREACH
            case ENETUNREACH:
#endif
#ifdef ENETRESET
            case ENETRESET:
#endif
                return static_cast<int>(SmbErrorCode::NotConnected);
            case ECONNREFUSED:
                return static_cast<int>(SmbErrorCode::ConnectionRefused);
            case ETIMEDOUT:
                return static_cast<int>(SmbErrorCode::TimedOut);
            case EBUSY:
                return static_cast<int>(SmbErrorCode::Busy);
            case ENOSPC:
                return static_cast<int>(SmbErrorCode::NoSpace);
            case EIO:
                return static_cast<int>(SmbErrorCode::Io);
            default:
                return static_cast<int>(SmbErrorCode::Unknown);
        }
    }

   private:
    static int fromMessage(const std::string& errorMessage) {
        if (errorMessage.empty()) {
            return static_cast<int>(SmbErrorCode::Unknown);
        }

        std::string upper = errorMessage;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (const auto ntStatus = extractNtStatusHex(upper); ntStatus.has_value()) {
            const int mapped = fromNtStatus(*ntStatus);
            if (mapped != static_cast<int>(SmbErrorCode::Unknown)) {
                return mapped;
            }
        }

        if (containsAny(upper, {"STATUS_OBJECT_NAME_COLLISION", "FILE_EXISTS", "EEXIST"})) {
            return static_cast<int>(SmbErrorCode::AlreadyExists);
        }

        if (containsAny(upper, {"STATUS_OBJECT_NAME_NOT_FOUND", "STATUS_OBJECT_PATH_NOT_FOUND", "STATUS_NO_SUCH_FILE", "NO SUCH FILE"})) {
            return static_cast<int>(SmbErrorCode::NotFound);
        }

        if (containsAny(upper, {"STATUS_NO_SUCH_USER", "STATUS_WRONG_PASSWORD", "STATUS_LOGON_FAILURE", "STATUS_ACCOUNT_RESTRICTION",
                                "STATUS_INVALID_LOGON_HOURS", "STATUS_INVALID_WORKSTATION", "STATUS_PASSWORD_EXPIRED",
                                "STATUS_ACCOUNT_DISABLED", "STATUS_ACCOUNT_EXPIRED", "STATUS_PASSWORD_MUST_CHANGE",
                                "STATUS_ACCOUNT_LOCKED_OUT", "AUTHENTICATION FAILED", "INVALID PASSWORD", "INVALID USERNAME"})) {
            return static_cast<int>(SmbErrorCode::AuthenticationFailed);
        }

        if (containsAny(upper, {"STATUS_ACCESS_DENIED", "ACCESS_DENIED", "PERMISSION DENIED"})) {
            return static_cast<int>(SmbErrorCode::AccessDenied);
        }

        if (contains(upper, "STATUS_NOT_A_DIRECTORY")) {
            return static_cast<int>(SmbErrorCode::NotDirectory);
        }

        if (contains(upper, "STATUS_FILE_IS_A_DIRECTORY")) {
            return static_cast<int>(SmbErrorCode::IsDirectory);
        }

        if (contains(upper, "STATUS_DIRECTORY_NOT_EMPTY")) {
            return static_cast<int>(SmbErrorCode::DirectoryNotEmpty);
        }

        if (containsAny(upper, {"CANCELLED", "CANCELED", "CONTEXT REQUEST CANCELLED", "CONNECTION TRANSITION CANCELLED"})) {
            return static_cast<int>(SmbErrorCode::Cancelled);
        }

        if (containsAny(upper, {"STATUS_IO_TIMEOUT", "TIMED OUT", "TIMEOUT EXPIRED"})) {
            return static_cast<int>(SmbErrorCode::TimedOut);
        }

        if (containsAny(upper, {"STATUS_CONNECTION_DISCONNECTED", "STATUS_NETWORK_NAME_DELETED", "BROKEN PIPE", "CLOSING OR DISCONNECTED",
                                "CONTEXT REQUEST FAILED", "POLLHUP", "POLLERR", "POLLNVAL", "SOCKET ERROR", "NO CONNECTION EXISTS",
                                "CONNECTION RESET", "CONNECTION ABORTED", "NETWORK IS UNREACHABLE"})) {
            return static_cast<int>(SmbErrorCode::NotConnected);
        }

        if (contains(upper, "STATUS_CONNECTION_REFUSED")) {
            return static_cast<int>(SmbErrorCode::ConnectionRefused);
        }

        if (containsAny(upper, {"STATUS_SHARING_VIOLATION", "RESOURCE BUSY"})) {
            return static_cast<int>(SmbErrorCode::Busy);
        }

        if (containsAny(upper, {"STATUS_DISK_FULL", "NO SPACE"})) {
            return static_cast<int>(SmbErrorCode::NoSpace);
        }

        if (containsAny(upper, {"STATUS_IO_DEVICE_ERROR", "I/O"})) {
            return static_cast<int>(SmbErrorCode::Io);
        }

        return static_cast<int>(SmbErrorCode::Unknown);
    }

    static int fromNtStatus(uint32_t ntStatus) {
        switch (ntStatus) {
            case 0xC0000034:  // STATUS_OBJECT_NAME_NOT_FOUND
            case 0xC000003A:  // STATUS_OBJECT_PATH_NOT_FOUND
                return static_cast<int>(SmbErrorCode::NotFound);
            case 0xC0000035:  // STATUS_OBJECT_NAME_COLLISION
                return static_cast<int>(SmbErrorCode::AlreadyExists);
            case 0xC0000022:  // STATUS_ACCESS_DENIED
                return static_cast<int>(SmbErrorCode::AccessDenied);
            case 0xC0000064:  // STATUS_NO_SUCH_USER
            case 0xC000006A:  // STATUS_WRONG_PASSWORD
            case 0xC000006D:  // STATUS_LOGON_FAILURE
            case 0xC000006E:  // STATUS_ACCOUNT_RESTRICTION
            case 0xC000006F:  // STATUS_INVALID_LOGON_HOURS
            case 0xC0000070:  // STATUS_INVALID_WORKSTATION
            case 0xC0000071:  // STATUS_PASSWORD_EXPIRED
            case 0xC0000072:  // STATUS_ACCOUNT_DISABLED
            case 0xC0000193:  // STATUS_ACCOUNT_EXPIRED
            case 0xC0000224:  // STATUS_PASSWORD_MUST_CHANGE
            case 0xC0000234:  // STATUS_ACCOUNT_LOCKED_OUT
                return static_cast<int>(SmbErrorCode::AuthenticationFailed);
            case 0xC0000103:  // STATUS_NOT_A_DIRECTORY
                return static_cast<int>(SmbErrorCode::NotDirectory);
            case 0xC00000BA:  // STATUS_FILE_IS_A_DIRECTORY
                return static_cast<int>(SmbErrorCode::IsDirectory);
            case 0xC0000101:  // STATUS_DIRECTORY_NOT_EMPTY
                return static_cast<int>(SmbErrorCode::DirectoryNotEmpty);
            case 0xC00000B5:  // STATUS_IO_TIMEOUT
                return static_cast<int>(SmbErrorCode::TimedOut);
            case 0xC000020C:  // STATUS_CONNECTION_DISCONNECTED
            case 0xC00000C9:  // STATUS_NETWORK_NAME_DELETED
                return static_cast<int>(SmbErrorCode::NotConnected);
            case 0xC000007F:  // STATUS_DISK_FULL
                return static_cast<int>(SmbErrorCode::NoSpace);
            case 0xC00000E9:  // STATUS_UNEXPECTED_IO_ERROR
            case 0xC0000185:  // STATUS_IO_DEVICE_ERROR / transport I/O
                return static_cast<int>(SmbErrorCode::Io);
            default:
                return static_cast<int>(SmbErrorCode::Unknown);
        }
    }

    static std::optional<uint32_t> extractNtStatusHex(const std::string& upperMessage) {
        size_t pos = 0;
        while ((pos = upperMessage.find("0X", pos)) != std::string::npos) {
            size_t hexStart = pos + 2;
            size_t hexEnd = hexStart;

            while (hexEnd < upperMessage.size() && std::isxdigit(static_cast<unsigned char>(upperMessage[hexEnd]))) {
                ++hexEnd;
            }

            const size_t hexLen = hexEnd - hexStart;
            if (hexLen >= 6 && hexLen <= 8) {
                try {
                    const auto parsed = static_cast<uint32_t>(std::stoul(upperMessage.substr(hexStart, hexLen), nullptr, 16));
                    return parsed;
                } catch (...) {
                }
            }

            pos = hexStart;
        }

        return std::nullopt;
    }

    static bool contains(const std::string& haystack, const char* needle) { return haystack.find(needle) != std::string::npos; }

    static bool containsAny(const std::string& haystack, std::initializer_list<const char*> needles) {
        for (const char* needle : needles) {
            if (contains(haystack, needle)) {
                return true;
            }
        }
        return false;
    }
};

}  // namespace react_native_smb
