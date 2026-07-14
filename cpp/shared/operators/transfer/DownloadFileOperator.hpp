#pragma once

#include <string>

#include "../OperatorBase.hpp"

namespace react_native_smb {

class DownloadFileOperator : public OperatorBase {
   public:
    DownloadFileOperator(std::string remotePath, std::string localPath) : remotePath_(std::move(remotePath)), localPath_(std::move(localPath)) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::DownloadFile; }
    void run() override;

   private:
    std::string remotePath_;
    std::string localPath_;
};

}  // namespace react_native_smb
