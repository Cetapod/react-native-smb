#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

class UploadFileOperator : public OperatorBase {
   public:
    UploadFileOperator(std::string localPath, std::string remotePath) : localPath_(std::move(localPath)), remotePath_(std::move(remotePath)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::UploadFile; }
    std::optional<AcquireMode> acquireMode() const override { return AcquireMode::Metadata; }
    void prepare() override;
    void run() override;

   private:
    std::string localPath_;
    std::string remotePath_;
    std::string local_;
    std::string remote_;
};

}  // namespace react_native_smb
