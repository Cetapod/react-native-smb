#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

// maxDepth == 0: streams one directory level in batches on the interactive slot
// (see DirectoryStreamer); the task's EntryStream holds the full result.
// maxDepth != 0 (-1 = unlimited): fans out across Metadata pool slots via
// listDirectoryParallel (bounded by maxConnections-1) and only publishes the final tree.
class ListDirectoryOperator : public OperatorBase {
   public:
    ListDirectoryOperator(std::string path, int maxDepth, bool includeSecurityDescriptor, int batchSize)
        : path_(std::move(path)), maxDepth_(maxDepth), includeSecurityDescriptor_(includeSecurityDescriptor), batchSize_(batchSize) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::ListDirectory; }
    void run() override;

   private:
    void runRecursive();
    void runStreaming();

    std::string path_;
    int maxDepth_;
    bool includeSecurityDescriptor_;
    int batchSize_;  // <= 0 uses DirectoryStreamOptions' default
};

}  // namespace react_native_smb
