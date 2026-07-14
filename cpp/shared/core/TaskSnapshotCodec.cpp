#include "TaskSnapshotCodec.hpp"

#include "SmbEnums.hpp"

namespace react_native_smb {

std::unordered_map<std::string, std::string> TaskSnapshotCodec::encode(const SmbTaskState& s) {
    return {
        {Keys::exists, "1"},
        {Keys::taskId, s.taskId},
        {Keys::kind, std::to_string(static_cast<int>(s.kind))},
        {Keys::operation, operationName(s.kind)},
        {Keys::status, std::to_string(static_cast<int>(s.status))},
        {Keys::determinate, isDeterminate(s.kind) ? "1" : "0"},
        {Keys::progress, std::to_string(s.progress)},
        {Keys::errorCode, std::to_string(s.errorCode)},
        {Keys::errorMessage, s.errorMessage},
        {Keys::sourcePath, s.sourcePath},
        {Keys::destinationPath, s.destinationPath},
        {Keys::bytesDone, std::to_string(s.bytesDone)},
        {Keys::totalBytes, std::to_string(s.totalBytes)},
        {Keys::bytesPerSecond, std::to_string(s.bytesPerSecond)},
        {Keys::etaSeconds, std::to_string(s.etaSeconds)},
        {Keys::startedAt, std::to_string(s.startedAt)},
        {Keys::updatedAt, std::to_string(s.updatedAt)},
        {Keys::endedAt, std::to_string(s.endedAt)},
    };
}

std::unordered_map<std::string, std::string> TaskSnapshotCodec::notFound() { return {{Keys::exists, "0"}}; }

}  // namespace react_native_smb
