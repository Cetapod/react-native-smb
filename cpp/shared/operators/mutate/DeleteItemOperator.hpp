#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

// Deletes a file or a directory tree. Directory deletion walks the tree and
// removes deepest-first (strict ordering), so it runs self-contained on Bulk
// Metadata slots rather than spawning child operators.
class DeleteItemOperator : public OperatorBase {
   public:
    explicit DeleteItemOperator(std::string path) : path_(std::move(path)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::DeleteItem; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Metadata; }
    void prepare() override;
    void run() override;

   private:
    std::string path_;
    std::string norm_;
};

}  // namespace react_native_smb
