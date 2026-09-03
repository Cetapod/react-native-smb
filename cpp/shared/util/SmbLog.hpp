#pragma once

// Native SMB logs are always enabled and use a small, consistent severity prefix.

#if defined(__APPLE__)
#include <os/log.h>
namespace react_native_smb {
inline os_log_t smb_log_handle() {
    static os_log_t h = os_log_create("com.cetapod.smb", "smb");
    return h;
}
}  // namespace react_native_smb
#define SMB_LOG_INFO(fmt, ...) os_log_with_type(::react_native_smb::smb_log_handle(), OS_LOG_TYPE_DEFAULT, "[smb][INFO] " fmt, ##__VA_ARGS__)
#define SMB_LOG_WARN(fmt, ...) os_log_with_type(::react_native_smb::smb_log_handle(), OS_LOG_TYPE_DEFAULT, "[smb][WARN] " fmt, ##__VA_ARGS__)
#define SMB_LOG_ERROR(fmt, ...) os_log_with_type(::react_native_smb::smb_log_handle(), OS_LOG_TYPE_ERROR, "[smb][ERROR] " fmt, ##__VA_ARGS__)
#elif defined(__ANDROID__)
#include <android/log.h>
#define SMB_LOG_INFO(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "smb", "[INFO] " fmt, ##__VA_ARGS__)
#define SMB_LOG_WARN(fmt, ...) __android_log_print(ANDROID_LOG_WARN, "smb", "[WARN] " fmt, ##__VA_ARGS__)
#define SMB_LOG_ERROR(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "smb", "[ERROR] " fmt, ##__VA_ARGS__)
#else
#include <cstdio>
#define SMB_LOG_INFO(fmt, ...) std::fprintf(stderr, "[smb][INFO] " fmt "\n", ##__VA_ARGS__)
#define SMB_LOG_WARN(fmt, ...) std::fprintf(stderr, "[smb][WARN] " fmt "\n", ##__VA_ARGS__)
#define SMB_LOG_ERROR(fmt, ...) std::fprintf(stderr, "[smb][ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
