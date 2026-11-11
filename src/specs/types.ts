/**
 * Structs and enums shared by Nitrogen HybridObject specs.
 * Domain C++ keeps its own models; bridge maps between them.
 * SmbFileInfo is intentionally absent — Nitrogen 0.37.2 cannot generate
 * recursive structs; it stays hand-converted via JSIConverter.
 */

export interface SmbCredentials {
  username: string
  password: string
}

export interface SmbShare {
  name: string
  comment: string
}

export interface SmbConnectionInfo {
  url: string
  server: string
  share: string
  isConnected: boolean
}

export interface SmbAce {
  aceType: string
  aceFlags: string[]
  mask: string[]
  sid: string
}

export interface SmbAcl {
  revision: number
  aceCount: number
  aces: SmbAce[]
}

export interface SmbSecurityDescriptor {
  revision: number
  control: string[]
  ownerSid: string
  groupSid: string
  dacl: SmbAcl
}

/** Mirrors domain SmbTaskStatus — numeric values must stay stable. */
export enum TaskStatus {
  Idle = 0,
  Pending = 1,
  Running = 2,
  Success = 3,
  Error = 4,
  Cancelled = 5,
}

/** Mirrors domain SmbOperatorKind — numeric values must stay stable. */
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

/** Mirrors domain SlotState — numeric values must stay stable. */
export enum SlotState {
  Idle = 0,
  Assigned = 1,
  Activating = 2,
}

export interface SmbTaskState {
  taskId: string
  kind: SmbOperatorKind
  status: TaskStatus
  determinate: boolean
  progress: number
  bytesDone: number
  totalBytes: number
  bytesPerSecond: number
  etaSeconds: number
  sourcePath: string
  destinationPath: string
  errorCode: number
  errorMessage: string
  startedAt: number
  updatedAt: number
  endedAt: number
}

export interface PoolSlotInfo {
  index: number
  interactiveOnly: boolean
  state: SlotState
  isConnected: boolean
  shareName: string
  taskId: string
  kind: SmbOperatorKind
}

export interface PoolInfo {
  poolSize: number
  slots: PoolSlotInfo[]
}
