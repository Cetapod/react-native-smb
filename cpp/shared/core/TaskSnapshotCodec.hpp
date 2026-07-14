#pragma once

#include <string>
#include <unordered_map>

#include "SmbTypes.hpp"

namespace react_native_smb {

// Single source of truth for the SmbTaskState -> Nitro string-map projection.
// The TS decoder (src/bridge/decode.ts) mirrors these key names.
class TaskSnapshotCodec {
   public:
    struct Keys {
        static constexpr const char* exists = "exists";  // "1" / "0" sentinel for getTask
        static constexpr const char* taskId = "taskId";
        static constexpr const char* kind = "kind";      // int(SmbOperatorKind)
        static constexpr const char* operation = "operation";  // debug only; TS ignores
        static constexpr const char* status = "status";  // int(SmbTaskStatus)
        static constexpr const char* determinate = "determinate";
        static constexpr const char* progress = "progress";
        static constexpr const char* errorCode = "errorCode";
        static constexpr const char* errorMessage = "errorMessage";
        static constexpr const char* sourcePath = "sourcePath";
        static constexpr const char* destinationPath = "destinationPath";
        static constexpr const char* bytesDone = "bytesDone";
        static constexpr const char* totalBytes = "totalBytes";
        static constexpr const char* bytesPerSecond = "bytesPerSecond";
        static constexpr const char* etaSeconds = "etaSeconds";
        static constexpr const char* startedAt = "startedAt";
        static constexpr const char* updatedAt = "updatedAt";
        static constexpr const char* endedAt = "endedAt";
    };

    static std::unordered_map<std::string, std::string> encode(const SmbTaskState& s);

    // Sentinel map for "task not found".
    static std::unordered_map<std::string, std::string> notFound();
};

}  // namespace react_native_smb
