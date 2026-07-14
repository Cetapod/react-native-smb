#pragma once

#include "../ReactNativeSmb.hpp"
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <string>

namespace react_native_smb {
class SmbSecurity {
   public:
    static std::string convertSidToString(struct smb2_sid* sid);

    static std::vector<std::string> convertControlFlags(const std::uint16_t& control, bool longForm = true);

    static std::vector<std::string> convertAccessMask(const uint32_t& mask);
    static std::vector<std::string> convertAccessMask(const uint32_t& mask, bool isDirectory);

    static std::string convertAceType(const uint8_t& aceType);
    static std::vector<std::string> convertAceFlags(const uint8_t& aceFlags);

    static SmbAcl convertAclToSmbAcl(struct smb2_acl* acl);
    static SmbAce convertAceToSmbAce(struct smb2_ace* ace);

    static SmbSecurityDescriptor processSecurityDescriptor(struct smb2_security_descriptor* sd);

   private:
    static void freeAcl(struct smb2_acl* acl);
};
}  // namespace react_native_smb
