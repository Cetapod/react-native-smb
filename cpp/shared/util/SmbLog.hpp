#pragma once

// Diagnostic logging: os_log on Apple, __android_log_print on Android, fprintf(stderr) elsewhere.

#if defined(__APPLE__)
#include <os/log.h>
namespace react_native_smb {
inline os_log_t smb_log_handle() {
    static os_log_t h = os_log_create("com.cetapod.smb", "smb");
    return h;
}
}  // namespace react_native_smb
#define SMB_LOG(fmt, ...) os_log(::react_native_smb::smb_log_handle(), "[smb] " fmt, ##__VA_ARGS__)
#elif defined(__ANDROID__)
#include <android/log.h>
#define SMB_LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "smb", fmt, ##__VA_ARGS__)
#else
#include <cstdio>
#define SMB_LOG(fmt, ...) std::fprintf(stderr, "[smb] " fmt "\n", ##__VA_ARGS__)
#endif
