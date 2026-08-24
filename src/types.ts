export interface SmbCredentials {
  readonly username: string;
  readonly password: string;
}

export interface SmbShareList {
  readonly name: string;
  readonly comment: string;
}

export interface SmbConnectionInfo {
  readonly url: string;
  readonly server: string;
  readonly share: string;
  readonly isConnected: boolean;
}

export enum SmbError {
  Unknown = 0,
  Cancelled = 1,
  InvalidArgument = 100,
  NotFound = 101,
  AlreadyExists = 102,
  AccessDenied = 103,
  NotDirectory = 104,
  IsDirectory = 105,
  DirectoryNotEmpty = 106,
  NotConnected = 200,
  ConnectionRefused = 201,
  TimedOut = 202,
  Busy = 300,
  NoSpace = 301,
  Io = 302,
}

export class SmbTaskError extends Error {
  readonly code: SmbError;
  readonly taskId?: string;

  constructor(message: string, code: SmbError = SmbError.Unknown, taskId?: string) {
    super(message);
    this.code = code;
    this.taskId = taskId;
  }
}

export interface SmbFileInfo {
  readonly name: string;
  readonly path: string;
  readonly size: number;
  readonly isDirectory: boolean;
  readonly modifiedAt: number;
  readonly accessedAt: number;
  readonly createdAt: number;
  readonly changedAt: number;
  readonly childCount?: number;
  readonly children?: SmbFileInfo[];
  readonly securityDescriptor?: SmbSecurityDescriptor;
}

export interface SmbAce {
  readonly aceType: string;
  readonly aceFlags: string[];
  readonly mask: string[];
  readonly sid: string;
}

export interface SmbAcl {
  readonly revision: number;
  readonly aceCount: number;
  readonly aces: SmbAce[];
}

export interface SmbSecurityDescriptor {
  readonly revision: number;
  readonly control: string[];
  readonly ownerSid: string;
  readonly groupSid: string;
  readonly dacl: SmbAcl;
}

export interface PathComponents {
  readonly parentDirectory: string;
  readonly baseFileName: string;
  readonly extension: string;
  readonly fullFileName: string;
}

export enum TaskStatus {
  Idle = 0,
  Pending = 1,
  Running = 2,
  Success = 3,
  Error = 4,
  Cancelled = 5,
}

export enum SmbOperatorKind {
  Initialize = 0,
  Connect = 1,
  ConnectShare = 2,
  Disconnect = 3,
  ListShares = 4,
  ListDirectory = 5,
  GetPathInfo = 6,
  GetSecurityDescriptor = 7,
  DownloadFile = 8,
  UploadFile = 9,
  CreateDirectory = 10,
  DeleteItem = 11,
  MoveItem = 12,
  RenameItem = 13,
  CopyItem = 14,
  DuplicateItem = 15,
}

export enum SlotState {
  Idle = 0,
  Assigned = 1,
  Activating = 2,
}

export interface SmbTaskState {
  readonly taskId: string;
  readonly kind: SmbOperatorKind;
  readonly status: TaskStatus;
  readonly determinate: boolean;
  readonly progress: number;
  readonly bytesDone: number;
  readonly totalBytes: number;
  readonly bytesPerSecond: number;
  readonly etaSeconds: number;
  readonly sourcePath: string;
  readonly destinationPath: string;
  readonly errorCode: number;
  readonly errorMessage: string;
  readonly startedAt: number;
  readonly updatedAt: number;
  readonly endedAt: number;
}

export interface PoolSlotInfo {
  readonly index: number;
  readonly interactiveOnly: boolean;
  readonly state: SlotState;
  readonly isConnected: boolean;
  readonly shareName: string;
  readonly taskId: string;
  readonly kind: SmbOperatorKind;
}

export interface PoolInfo {
  readonly poolSize: number;
  readonly slots: PoolSlotInfo[];
}

export interface NativeSmbTask {
  cancel(): void;
  get(): Record<string, string>;
  subscribe(listener: (snapshot: Record<string, string>) => void): string;
  unsubscribe(subscriptionId: string): void;
  getResultValue(): unknown;
}
