import type { NativeSmbListTask as NitroNativeSmbListTask } from './specs/NativeSmbListTask.nitro'
import type { NativeSmbTask as NitroNativeSmbTask } from './specs/NativeSmbTask.nitro'

export type {
  SmbCredentials,
  SmbShare,
  SmbConnectionInfo,
  SmbAce,
  SmbAcl,
  SmbSecurityDescriptor,
} from './specs/types'

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
  AuthenticationFailed = 203,
  Busy = 300,
  NoSpace = 301,
  Io = 302,
}

export class SmbTaskError extends Error {
  readonly code: SmbError
  readonly taskId?: string

  constructor(message: string, code: SmbError = SmbError.Unknown, taskId?: string) {
    super(message)
    this.code = code
    this.taskId = taskId
  }
}

export interface SmbFileInfo {
  readonly name: string
  readonly path: string
  readonly size: number
  readonly isDirectory: boolean
  readonly modifiedAt: number
  readonly accessedAt: number
  readonly createdAt: number
  readonly changedAt: number
  readonly childCount?: number
  readonly children?: SmbFileInfo[]
  readonly securityDescriptor?: import('./specs/types').SmbSecurityDescriptor
}

export interface ListDirectoryOptions {
  /** Nested levels to include as `children`: 0 = this level only (default), -1 = unlimited. */
  readonly maxDepth?: number
  /** Fetch each item's security descriptor (default `false`). */
  readonly includeSecurityDescriptor?: boolean
  /** Top-level entries per streamed batch (default 10). */
  readonly batchSize?: number
}

export interface PathComponents {
  readonly parentDirectory: string
  readonly baseFileName: string
  readonly extension: string
  readonly fullFileName: string
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
  readonly taskId: string
  readonly kind: SmbOperatorKind
  readonly status: TaskStatus
  readonly determinate: boolean
  readonly progress: number
  readonly bytesDone: number
  readonly totalBytes: number
  readonly bytesPerSecond: number
  readonly etaSeconds: number
  readonly sourcePath: string
  readonly destinationPath: string
  readonly errorCode: number
  readonly errorMessage: string
  readonly startedAt: number
  readonly updatedAt: number
  readonly endedAt: number
}

export interface PoolSlotInfo {
  readonly index: number
  readonly interactiveOnly: boolean
  readonly state: SlotState
  readonly isConnected: boolean
  readonly shareName: string
  readonly taskId: string
  readonly kind: SmbOperatorKind
}

export interface PoolInfo {
  readonly poolSize: number
  readonly slots: PoolSlotInfo[]
}

/**
 * Native HybridObject for one SMB task.
 * Nitrogen generates cancel/get/subscribe/…; getResultValue is a raw JSI method.
 */
export type NativeSmbTask = NitroNativeSmbTask & {
  getResultValue(): unknown
}

/**
 * Listing HybridObject. Nitrogen generates entry signals; drainEntries is raw
 * because SmbFileInfo is recursive.
 */
export type NativeSmbListTask = NitroNativeSmbListTask &
  NativeSmbTask & {
    /**
     * Payload-free wake for new entries. Call {@link drainEntries} to pull every
     * entry appended since the last drain (including a late-subscribe replay).
     */
    drainEntries(subscriptionId: string): SmbFileInfo[]
  }
