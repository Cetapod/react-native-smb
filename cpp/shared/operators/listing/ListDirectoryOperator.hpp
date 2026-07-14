#pragma once

#include <string>
#include <vector>

#include "../../ReactNativeSmb.hpp"  // SmbFileInfo
#include "../Operator.hpp"

namespace react_native_smb {

class ListDirectoryOperator : public Operator<std::vector<SmbFileInfo>> {
   public:
    ListDirectoryOperator(std::string path, bool recursive, int maxDepth) : path_(std::move(path)), recursive_(recursive), maxDepth_(maxDepth) {}

    SmbOperatorKind kind() const override { return SmbOperatorKind::ListDirectory; }
    void run() override;

   private:
    std::string path_;
    bool recursive_;
    int maxDepth_;
};

}  // namespace react_native_smb
