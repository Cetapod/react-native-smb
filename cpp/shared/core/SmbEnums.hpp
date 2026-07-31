#pragma once

namespace react_native_smb {

// One operator class per kind. The enum is the single source of truth for the
// human-readable operation name (operationName) and whether byte-progress is
// meaningful (isDeterminate).
enum class SmbOperatorKind {
    // Connection Management
    Initialize,
    Connect,
    ConnectShare,
    Disconnect,
    ListShares,
    // Listing & Info
    ListDirectory,
    GetPathInfo,
    GetSecurityDescriptor,
    // File Transfers
    DownloadFile,
    UploadFile,
    // Mutating File Operations
    CreateDirectory,
    DeleteItem,
    MoveItem,
    RenameItem,
    CopyItem,
    DuplicateItem,
};

// Task lifecycle status. Index order matters: statusName[] mirrors it.
enum class SmbTaskStatus {
    Idle,
    Pending,
    Running,
    Success,
    Error,
    Cancelled,
};

// Single source of truth: operation string is derived from the enum, never
// typed by hand. HybridSMB passes no literal operation string.
constexpr const char* operationName(SmbOperatorKind k) {
    switch (k) {
        case SmbOperatorKind::Initialize:
            return "initialize";
        case SmbOperatorKind::Connect:
            return "connect";
        case SmbOperatorKind::ConnectShare:
            return "connectShare";
        case SmbOperatorKind::Disconnect:
            return "disconnect";
        case SmbOperatorKind::ListShares:
            return "listShares";
        case SmbOperatorKind::ListDirectory:
            return "listDirectory";
        case SmbOperatorKind::GetPathInfo:
            return "getPathInfo";
        case SmbOperatorKind::GetSecurityDescriptor:
            return "getSecurityDescriptor";
        case SmbOperatorKind::DownloadFile:
            return "downloadFile";
        case SmbOperatorKind::UploadFile:
            return "uploadFile";
        case SmbOperatorKind::CreateDirectory:
            return "createDirectory";
        case SmbOperatorKind::DeleteItem:
            return "deleteItem";
        case SmbOperatorKind::MoveItem:
            return "moveItem";
        case SmbOperatorKind::RenameItem:
            return "renameItem";
        case SmbOperatorKind::CopyItem:
            return "copyItem";
        case SmbOperatorKind::DuplicateItem:
            return "duplicateItem";
    }
    return "unknown";
}

// Transfers carry meaningful byte progress; metadata ops do not. The UI uses
// this to choose a determinate bar vs. an indeterminate spinner.
constexpr bool isDeterminate(SmbOperatorKind k) {
    switch (k) {
        case SmbOperatorKind::DownloadFile:
        case SmbOperatorKind::UploadFile:
        case SmbOperatorKind::CopyItem:
        case SmbOperatorKind::DuplicateItem:
            return true;
        default:
            return false;
    }
}

constexpr bool isTransferKind(SmbOperatorKind k) {
    switch (k) {
        case SmbOperatorKind::DownloadFile:
        case SmbOperatorKind::UploadFile:
        case SmbOperatorKind::CopyItem:
        case SmbOperatorKind::DuplicateItem:
            return true;
        default:
            return false;
    }
}

// Connection/session lifecycle ops stay on the primary path and must not grow
// the pool. Listing, transfers, and mutations may create a general slot when
// no idle slot is available (up to maxConnections).
constexpr bool canGrowPoolSlot(SmbOperatorKind k) {
    switch (k) {
        case SmbOperatorKind::Initialize:
        case SmbOperatorKind::Connect:
        case SmbOperatorKind::ConnectShare:
        case SmbOperatorKind::Disconnect:
        case SmbOperatorKind::ListShares:
            return false;
        default:
            return true;
    }
}

// Lowercase status name for the JS bridge. Index mirrors SmbTaskStatus.
constexpr const char* statusName(SmbTaskStatus s) {
    switch (s) {
        case SmbTaskStatus::Idle:
            return "idle";
        case SmbTaskStatus::Pending:
            return "pending";
        case SmbTaskStatus::Running:
            return "running";
        case SmbTaskStatus::Success:
            return "success";
        case SmbTaskStatus::Error:
            return "error";
        case SmbTaskStatus::Cancelled:
            return "cancelled";
    }
    return "unknown";
}

}  // namespace react_native_smb
