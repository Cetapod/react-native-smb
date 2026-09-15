import { SmbOperatorKind, TaskStatus } from './types';

export function statusLabel(status: TaskStatus): string {
  switch (status) {
    case TaskStatus.Idle:
      return 'idle';
    case TaskStatus.Pending:
      return 'pending';
    case TaskStatus.Running:
      return 'running';
    case TaskStatus.Success:
      return 'success';
    case TaskStatus.Error:
      return 'error';
    case TaskStatus.Cancelled:
      return 'cancelled';
    default:
      return 'unknown';
  }
}

export function operationLabel(kind: SmbOperatorKind): string {
  switch (kind) {
    case SmbOperatorKind.Initialize:
      return 'Initialize';
    case SmbOperatorKind.Connect:
      return 'Connect';
    case SmbOperatorKind.ConnectShare:
      return 'Connect Share';
    case SmbOperatorKind.Disconnect:
      return 'Disconnect';
    case SmbOperatorKind.ListShares:
      return 'List Shares';
    case SmbOperatorKind.ListDirectory:
      return 'List Directory';
    case SmbOperatorKind.GetPathInfo:
      return 'Get Info';
    case SmbOperatorKind.GetSecurityDescriptor:
      return 'Get ACL';
    case SmbOperatorKind.DownloadFile:
      return 'Download';
    case SmbOperatorKind.UploadFile:
      return 'Upload';
    case SmbOperatorKind.CreateDirectory:
      return 'Create Folder';
    case SmbOperatorKind.DeleteItem:
      return 'Delete';
    case SmbOperatorKind.MoveItem:
      return 'Move';
    case SmbOperatorKind.RenameItem:
      return 'Rename';
    case SmbOperatorKind.CopyItem:
      return 'Copy';
    case SmbOperatorKind.DuplicateItem:
      return 'Duplicate';
    default:
      return 'Unknown';
  }
}

export function isTransferKind(kind: SmbOperatorKind): boolean {
  return (
    kind === SmbOperatorKind.DownloadFile ||
    kind === SmbOperatorKind.UploadFile ||
    kind === SmbOperatorKind.CopyItem ||
    kind === SmbOperatorKind.DuplicateItem
  );
}

export function isDeterminateKind(kind: SmbOperatorKind): boolean {
  return isTransferKind(kind);
}

export function isTerminalStatus(status: TaskStatus): boolean {
  return (
    status === TaskStatus.Success ||
    status === TaskStatus.Error ||
    status === TaskStatus.Cancelled
  );
}