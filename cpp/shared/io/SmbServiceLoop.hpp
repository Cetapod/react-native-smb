#pragma once

#include <poll.h>

#include <cerrno>
#include <ctime>
#include <functional>
#include <string>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "../util/SmbException.hpp"

namespace react_native_smb {

// Services an exclusively held context until `done()` holds, so several queued
// async requests can complete in one wait. Throws on socket failure, or after
// `idleSeconds` without any traffic. `label` prefixes error messages.
inline void serviceUntil(smb2_context* ctx, const std::function<bool()>& done, const std::string& label, int idleSeconds = 10) {
    int idle = 0;
    while (!done()) {
        pollfd pfd{};
        pfd.fd = smb2_get_fd(ctx);
        if (pfd.fd < 0) SmbException::raise(SmbErrorCode::NotConnected, label + ": invalid file descriptor from libsmb2");
        pfd.events = static_cast<short>(smb2_which_events(ctx));

        const int result = ::poll(&pfd, 1, 1000);
        if (result < 0) {
            if (errno == EINTR) continue;
            SmbException::raiseFromErrno(errno, label + ": poll failed");
        }
        if (result == 0) {
            if (++idle >= idleSeconds) SmbException::raise(SmbErrorCode::TimedOut, label + " timed out");
            continue;
        }
        idle = 0;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            SmbException::raise(SmbErrorCode::NotConnected, label + ": socket poll failure (revents=" + std::to_string(pfd.revents) + ")");
        }
        if (smb2_service(ctx, pfd.revents) < 0) {
            SmbException::raiseFromSmb(ctx, -1, label + ": smb2_service failed: " + std::string(smb2_get_error(ctx)), SmbErrorCode::NotConnected);
        }
    }
}

}  // namespace react_native_smb
