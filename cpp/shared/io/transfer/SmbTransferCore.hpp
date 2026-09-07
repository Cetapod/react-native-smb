#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include <smb2/smb2.h>

#include "../../core/CancellationToken.hpp"
#include "../../util/SmbException.hpp"

namespace react_native_smb {

class SmbConnectionManager;

namespace detail {

constexpr int kMaxInFlight = 8;

struct TransferFailure {
    bool errored = false;
    SmbErrorCode code{SmbErrorCode::Unknown};
    std::string message;

    void fail(SmbErrorCode errorCode, std::string errorMessage) {
        if (errored) {
            return;
        }
        errored = true;
        code = errorCode;
        message = std::move(errorMessage);
    }
};

struct TransferCore {
    smb2_context* ctx = nullptr;
    int64_t fileSize = 0;
    uint32_t chunkSize = 0;
    uint64_t nextOffset = 0;
    int64_t completedBytes = 0;
    int64_t lastProgressBytes = 0;
    int inFlight = 0;
    CancellationToken cancel;
    std::function<void(double, double)> progressHandler;
    TransferFailure failure;

    void emitProgress(int64_t current, int64_t total);
};

enum class PollLoopResult { Done, TimedOut, PollFailed };

SmbErrorCode mapSmbResult(int result, const std::string& message);
SmbErrorCode mapErrnoOrIo(int error);
uint32_t capAsyncChunkSize(uint32_t negotiatedSize);
[[noreturn]] void raiseTransferError(SmbErrorCode code, const std::string& message);

// The callbacks hold pointers to endpoint stack state. On ordinary failures this
// keeps polling until all submitted requests have invoked those callbacks.
PollLoopResult runTransfer(TransferCore& core,
                           SmbConnectionManager& manager,
                           const std::function<void()>& initialFill,
                           const std::function<bool()>& done,
                           const std::function<bool()>& stuck,
                           const std::function<void()>& markCancelled,
                           const std::function<void()>& clearHandles,
                           const std::string& failurePrefix);

}  // namespace detail
}  // namespace react_native_smb
