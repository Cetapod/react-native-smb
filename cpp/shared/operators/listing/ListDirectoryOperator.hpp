#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

// Lists through DirectoryLister on one pool slot: maxDepth 0 uses the
// interactive slot, nested listings (maxDepth != 0, -1 = unlimited) a Metadata
// slot so they never hold the UI slot. Top-level entries stream in batches; the
// task's EntryStream holds the full result.
class ListDirectoryOperator : public OperatorBase {
   public:
    ListDirectoryOperator(std::string path, int maxDepth, bool includeSecurityDescriptor, int batchSize)
        : path_(std::move(path)), maxDepth_(maxDepth), includeSecurityDescriptor_(includeSecurityDescriptor), batchSize_(batchSize) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::ListDirectory; }
    void run() override;

   private:
    std::string path_;
    int maxDepth_;
    bool includeSecurityDescriptor_;
    int batchSize_;  // <= 0 uses DirectoryListOptions' default
};

}  // namespace react_native_smb
