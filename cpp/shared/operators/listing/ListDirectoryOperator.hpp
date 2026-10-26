#pragma once

#include <string>
#include <vector>

#include "../../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../Operator.hpp"

namespace react_native_smb {

// Non-recursive listings stream entries in batches on the interactive slot
// (see DirectoryStreamer); the task's EntryStream holds the full result.
// Recursive listings fan out across Metadata pool slots via listDirectoryParallel
// (bounded by maxConnections-1) and only produce the final result.
class ListDirectoryOperator : public Operator<std::vector<SmbFileInfo>> {
   public:
    ListDirectoryOperator(std::string path, bool recursive, int maxDepth, bool includeSecurityDescriptor, int batchSize)
        : path_(std::move(path)),
          recursive_(recursive),
          maxDepth_(maxDepth),
          includeSecurityDescriptor_(includeSecurityDescriptor),
          batchSize_(batchSize) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::ListDirectory; }
    void run() override;

   private:
    void runRecursive();
    void runStreaming();

    std::string path_;
    bool recursive_;
    int maxDepth_;
    bool includeSecurityDescriptor_;
    int batchSize_;
};

}  // namespace react_native_smb
