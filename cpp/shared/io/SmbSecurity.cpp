#include "SmbSecurity.hpp"

#include <iomanip>
#include <sstream>

namespace react_native_smb {

std::string SmbSecurity::convertSidToString(struct smb2_sid* sid) {
    if (!sid) {
        return "";
    }

    std::stringstream ss;
    ss << "S-1";

    // Convert identifier authority
    uint64_t ia = 0;
    for (int i = 0; i < SID_ID_AUTH_LEN; i++) {
        ia <<= 8;
        ia |= sid->id_auth[i];
    }

    if (ia <= 0xffffffff) {
        ss << "-" << ia;
    } else {
        ss << "-0x" << std::hex << ia;
    }

    // Convert sub-authorities
    for (int i = 0; i < sid->sub_auth_count; i++) {
        ss << "-" << sid->sub_auth[i];
    }

    return ss.str();
}

std::vector<std::string> SmbSecurity::convertControlFlags(const std::uint16_t& control, bool longForm) {
    std::vector<std::string> flags;

    if (control == 0) {
        flags.push_back("None");
        return flags;
    }

    if (control & SMB2_SD_CONTROL_SR) flags.push_back(longForm ? "SELF_RELATIVE" : "SR");
    if (control & SMB2_SD_CONTROL_RM) flags.push_back(longForm ? "RM_CONTROL_VALID" : "RM");
    if (control & SMB2_SD_CONTROL_PS) flags.push_back(longForm ? "SACL_PROTECTED" : "PS");
    if (control & SMB2_SD_CONTROL_PD) flags.push_back(longForm ? "DACL_PROTECTED" : "PD");
    if (control & SMB2_SD_CONTROL_SI) flags.push_back(longForm ? "SACL_AUTO_INHERITED" : "SI");
    if (control & SMB2_SD_CONTROL_DI) flags.push_back(longForm ? "DACL_AUTO_INHERITED" : "DI");
    if (control & SMB2_SD_CONTROL_SC) flags.push_back(longForm ? "SACL_COMPUTED_INHERITANCE_REQUIRED" : "SC");
    if (control & SMB2_SD_CONTROL_DC) flags.push_back(longForm ? "DACL_COMPUTED_INHERITANCE_REQUIRED" : "DC");
    if (control & SMB2_SD_CONTROL_DT) flags.push_back(longForm ? "DACL_TRUSTED" : "DT");
    if (control & SMB2_SD_CONTROL_SS) flags.push_back(longForm ? "SERVER_SECURITY" : "SS");
    if (control & SMB2_SD_CONTROL_SD) flags.push_back(longForm ? "SACL_DEFAULTED" : "SD");
    if (control & SMB2_SD_CONTROL_SP) flags.push_back(longForm ? "SACL_PRESENT" : "SP");
    if (control & SMB2_SD_CONTROL_DD) flags.push_back(longForm ? "DACL_DEFAULTED" : "DD");
    if (control & SMB2_SD_CONTROL_DP) flags.push_back(longForm ? "DACL_PRESENT" : "DP");
    if (control & SMB2_SD_CONTROL_GD) flags.push_back(longForm ? "GROUP_DEFAULTED" : "GD");
    if (control & SMB2_SD_CONTROL_OD) flags.push_back(longForm ? "OWNER_DEFAULTED" : "OD");

    if (flags.empty()) {
        std::stringstream ss;
        ss << "0x" << std::hex << control;
        flags.push_back(ss.str());
    }

    return flags;
}

std::vector<std::string> SmbSecurity::convertAccessMask(const uint32_t& mask) {
    std::vector<std::string> permissions;

    if (mask == 0) {
        permissions.push_back("None");
        return permissions;
    }

    // Generic access rights (highest priority)
    if (mask & SMB2_GENERIC_ALL) permissions.push_back("GENERIC_ALL");
    if (mask & SMB2_GENERIC_EXECUTE) permissions.push_back("GENERIC_EXECUTE");
    if (mask & SMB2_GENERIC_WRITE) permissions.push_back("GENERIC_WRITE");
    if (mask & SMB2_GENERIC_READ) permissions.push_back("GENERIC_READ");

    // Standard access rights
    if (mask & SMB2_MAXIMUM_ALLOWED) permissions.push_back("MAXIMUM_ALLOWED");
    if (mask & SMB2_ACCESS_SYSTEM_SECURITY) permissions.push_back("ACCESS_SYSTEM_SECURITY");
    if (mask & SMB2_SYNCHRONIZE) permissions.push_back("SYNCHRONIZE");
    if (mask & SMB2_WRITE_OWNER) permissions.push_back("WRITE_OWNER");
    if (mask & SMB2_WRITE_DACL) permissions.push_back("WRITE_DACL");
    if (mask & SMB2_READ_CONTROL) permissions.push_back("READ_CONTROL");
    if (mask & SMB2_DELETE) permissions.push_back("DELETE");

    // File/directory specific access rights
    if (mask & SMB2_FILE_WRITE_ATTRIBUTES) permissions.push_back("WRITE_ATTRIBUTES");
    if (mask & SMB2_FILE_READ_ATTRIBUTES) permissions.push_back("READ_ATTRIBUTES");
    if (mask & SMB2_FILE_DELETE_CHILD) permissions.push_back("DELETE_CHILD");
    if (mask & SMB2_FILE_EXECUTE) permissions.push_back("EXECUTE");
    if (mask & SMB2_FILE_WRITE_EA) permissions.push_back("WRITE_EA");
    if (mask & SMB2_FILE_READ_EA) permissions.push_back("READ_EA");

    // File specific rights
    if (mask & SMB2_FILE_APPEND_DATA) permissions.push_back("APPEND_DATA");
    if (mask & SMB2_FILE_WRITE_DATA) permissions.push_back("WRITE_DATA");
    if (mask & SMB2_FILE_READ_DATA) permissions.push_back("READ_DATA");

    if (permissions.empty()) {
        std::stringstream ss;
        ss << "0x" << std::hex << mask;
        permissions.push_back(ss.str());
    }

    return permissions;
}

std::vector<std::string> SmbSecurity::convertAccessMask(const uint32_t& mask, bool isDirectory) {
    std::vector<std::string> permissions;

    if (mask == 0) {
        permissions.push_back("None");
        return permissions;
    }

    // Generic access rights (highest priority)
    if (mask & SMB2_GENERIC_ALL) permissions.push_back("GENERIC_ALL");
    if (mask & SMB2_GENERIC_EXECUTE) permissions.push_back("GENERIC_EXECUTE");
    if (mask & SMB2_GENERIC_WRITE) permissions.push_back("GENERIC_WRITE");
    if (mask & SMB2_GENERIC_READ) permissions.push_back("GENERIC_READ");

    // Standard access rights
    if (mask & SMB2_MAXIMUM_ALLOWED) permissions.push_back("MAXIMUM_ALLOWED");
    if (mask & SMB2_ACCESS_SYSTEM_SECURITY) permissions.push_back("ACCESS_SYSTEM_SECURITY");
    if (mask & SMB2_SYNCHRONIZE) permissions.push_back("SYNCHRONIZE");
    if (mask & SMB2_WRITE_OWNER) permissions.push_back("WRITE_OWNER");
    if (mask & SMB2_WRITE_DACL) permissions.push_back("WRITE_DACL");
    if (mask & SMB2_READ_CONTROL) permissions.push_back("READ_CONTROL");
    if (mask & SMB2_DELETE) permissions.push_back("DELETE");

    // File/directory specific access rights
    if (mask & SMB2_FILE_WRITE_ATTRIBUTES) permissions.push_back("WRITE_ATTRIBUTES");
    if (mask & SMB2_FILE_READ_ATTRIBUTES) permissions.push_back("READ_ATTRIBUTES");
    if (mask & SMB2_FILE_DELETE_CHILD) permissions.push_back("DELETE_CHILD");
    if (mask & SMB2_FILE_WRITE_EA) permissions.push_back("WRITE_EA");
    if (mask & SMB2_FILE_READ_EA) permissions.push_back("READ_EA");

    // Context-specific rights
    if (isDirectory) {
        if (mask & SMB2_FILE_LIST_DIRECTORY) permissions.push_back("LIST_DIRECTORY");
        if (mask & SMB2_FILE_ADD_FILE) permissions.push_back("ADD_FILE");
        if (mask & SMB2_FILE_ADD_SUBDIRECTORY) permissions.push_back("ADD_SUBDIRECTORY");
        if (mask & SMB2_FILE_TRAVERSE) permissions.push_back("TRAVERSE");
    } else {
        if (mask & SMB2_FILE_READ_DATA) permissions.push_back("READ_DATA");
        if (mask & SMB2_FILE_WRITE_DATA) permissions.push_back("WRITE_DATA");
        if (mask & SMB2_FILE_APPEND_DATA) permissions.push_back("APPEND_DATA");
        if (mask & SMB2_FILE_EXECUTE) permissions.push_back("EXECUTE");
    }

    if (permissions.empty()) {
        std::stringstream ss;
        ss << "0x" << std::hex << mask;
        permissions.push_back(ss.str());
    }

    return permissions;
}

std::string SmbSecurity::convertAceType(const uint8_t& aceType) {
    switch (aceType) {
        case SMB2_ACCESS_ALLOWED_ACE_TYPE:
            return "ACCESS_ALLOWED";
        case SMB2_ACCESS_DENIED_ACE_TYPE:
            return "ACCESS_DENIED";
        case SMB2_SYSTEM_AUDIT_ACE_TYPE:
            return "SYSTEM_AUDIT";
        case SMB2_ACCESS_ALLOWED_OBJECT_ACE_TYPE:
            return "ACCESS_ALLOWED_OBJECT";
        case SMB2_ACCESS_DENIED_OBJECT_ACE_TYPE:
            return "ACCESS_DENIED_OBJECT";
        case SMB2_SYSTEM_AUDIT_OBJECT_ACE_TYPE:
            return "SYSTEM_AUDIT_OBJECT";
        case SMB2_ACCESS_ALLOWED_CALLBACK_ACE_TYPE:
            return "ACCESS_ALLOWED_CALLBACK";
        case SMB2_ACCESS_DENIED_CALLBACK_ACE_TYPE:
            return "ACCESS_DENIED_CALLBACK";
        case SMB2_SYSTEM_MANDATORY_LABEL_ACE_TYPE:
            return "SYSTEM_MANDATORY_LABEL";
        case SMB2_SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE:
            return "SYSTEM_RESOURCE_ATTRIBUTE";
        case SMB2_SYSTEM_SCOPED_POLICY_ID_ACE_TYPE:
            return "SYSTEM_SCOPED_POLICY_ID";
        default:
            std::stringstream ss;
            ss << "UNKNOWN_TYPE_0x" << std::hex << static_cast<int>(aceType);
            return ss.str();
    }
}

std::vector<std::string> SmbSecurity::convertAceFlags(const uint8_t& aceFlags) {
    std::vector<std::string> flags;

    if (aceFlags == 0) {
        flags.push_back("None");
        return flags;
    }

    if (aceFlags & SMB2_OBJECT_INHERIT_ACE) flags.push_back("OBJECT_INHERIT");
    if (aceFlags & SMB2_CONTAINER_INHERIT_ACE) flags.push_back("CONTAINER_INHERIT");
    if (aceFlags & SMB2_NO_PROPAGATE_INHERIT_ACE) flags.push_back("NO_PROPAGATE_INHERIT");
    if (aceFlags & SMB2_INHERIT_ONLY_ACE) flags.push_back("INHERIT_ONLY");
    if (aceFlags & SMB2_INHERITED_ACE) flags.push_back("INHERITED");
    if (aceFlags & SMB2_SUCCESSFUL_ACCESS_ACE_FLAG) flags.push_back("SUCCESSFUL_ACCESS");
    if (aceFlags & SMB2_FAILED_ACCESS_ACE_FLAG) flags.push_back("FAILED_ACCESS");

    if (flags.empty()) {
        std::stringstream ss;
        ss << "0x" << std::hex << static_cast<int>(aceFlags);
        flags.push_back(ss.str());
    }

    return flags;
}

SmbAcl SmbSecurity::convertAclToSmbAcl(struct smb2_acl* acl) {
    SmbAcl result;
    if (!acl) {
        return result;
    }

    result.revision = acl->revision;
    result.aceCount = acl->ace_count;

    struct smb2_ace* ace = acl->aces;
    int aceIndex = 0;

    while (ace && aceIndex < acl->ace_count) {
        try {
            result.aces.push_back(convertAceToSmbAce(ace));
            ace = ace->next;
            aceIndex++;
        } catch (const std::exception& e) {
            break;
        }
    }

    result.aceCount = static_cast<uint16_t>(result.aces.size());

    return result;
}

SmbAce SmbSecurity::convertAceToSmbAce(struct smb2_ace* ace) {
    SmbAce result;
    if (!ace) {
        return result;
    }

    result.aceType = convertAceType(ace->ace_type);
    result.aceFlags = convertAceFlags(ace->ace_flags);
    result.mask = convertAccessMask(ace->mask);
    result.sid = SmbSecurity::convertSidToString(ace->sid);

    return result;
}

SmbSecurityDescriptor SmbSecurity::processSecurityDescriptor(struct smb2_security_descriptor* sd) {
    SmbSecurityDescriptor result{0, {}, "", "", SmbAcl{0, 0, {}}};

    if (!sd) {
        return result;
    }

    // Set revision
    result.revision = sd->revision;

    if (sd->control) {
        result.control = convertControlFlags(sd->control, true);
    } else {
        result.control = {"None"};
    }

    // Convert Owner, Group, and DACL
    if (sd->owner) {
        result.ownerSid = convertSidToString(sd->owner);
    } else {
        result.ownerSid = "";
    }

    if (sd->group) {
        result.groupSid = convertSidToString(sd->group);
    } else {
        result.groupSid = "";
    }

    if (sd->dacl) {
        result.dacl = convertAclToSmbAcl(sd->dacl);
    } else {
        result.dacl = SmbAcl{0, 0, {}};
    }

    return result;
}

void SmbSecurity::freeAcl(struct smb2_acl* acl) {
    if (acl) {
        free(acl);
    }
}
}  // namespace react_native_smb
