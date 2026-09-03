#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include "SmbErrorMapper.hpp"

namespace react_native_smb {

// Typed native exception. Prefer throwing this at the error origin so
// SmbTask::launchOp can settle with a stable SmbErrorCode instead of
// re-guessing from message text.
class SmbException : public std::runtime_error {
   public:
    SmbException(SmbErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    SmbException(int code, std::string message)
        : std::runtime_error(std::move(message)), code_(static_cast<SmbErrorCode>(code)) {}

    SmbErrorCode code() const { return code_; }
    int codeInt() const { return static_cast<int>(code_); }

    [[noreturn]] static void raise(SmbErrorCode code, std::string message) {
        throw SmbException(code, std::move(message));
    }

    [[noreturn]] static void raise(int code, std::string message) {
        throw SmbException(code, std::move(message));
    }

    [[noreturn]] static void raiseFromErrnoResult(int resultCode, std::string message) {
        throw SmbException(SmbErrorMapper::fromErrnoResult(resultCode), std::move(message));
    }

    [[noreturn]] static void raiseFromErrno(int errnoCode, std::string message) {
        throw SmbException(SmbErrorMapper::fromErrno(errnoCode), std::move(message));
    }

   private:
    SmbErrorCode code_{SmbErrorCode::Unknown};
};

}  // namespace react_native_smb
