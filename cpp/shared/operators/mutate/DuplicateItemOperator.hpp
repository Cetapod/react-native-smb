#pragma once

#include <string>

#include "../Operator.hpp"

namespace react_native_smb {

// Duplicates a file or directory into the same parent with a unique
// "<name> copy" suffix. Returns the new path. Self-contained (reuses the same
// tree-copy logic as CopyItemOperator).
class DuplicateItemOperator : public Operator<std::string> {
   public:
    explicit DuplicateItemOperator(std::string path) : path_(std::move(path)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::DuplicateItem; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Metadata; }
    void prepare() override;
    void run() override;

   private:
    std::string path_;
    std::string sourcePath_;
};

}  // namespace react_native_smb
