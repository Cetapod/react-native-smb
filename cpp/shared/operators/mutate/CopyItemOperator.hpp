#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

// Copies a file or a directory tree (recursive). Self-contained: mirrors the
// destination skeleton and copies files via the async pipelined copy. Progress
// is aggregated into a single cumulative byte count.
class CopyItemOperator : public OperatorBase {
   public:
    CopyItemOperator(std::string fromPath, std::string toPath, bool recursive = true) : fromPath_(std::move(fromPath)), toPath_(std::move(toPath)), recursive_(recursive) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::CopyItem; }
    void run() override;

   private:
    std::string fromPath_;
    std::string toPath_;
    bool recursive_;
};

}  // namespace react_native_smb
