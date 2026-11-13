import type { NativeSmbListTask as NitroNativeSmbListTask } from './specs/NativeSmbListTask.nitro'
import type { NativeSmbTask as NitroNativeSmbTask } from './specs/NativeSmbTask.nitro'

export type {
  SmbCredentials,
  SmbShare,
  SmbConnectionInfo,
  SmbAce,
  SmbAcl,
  SmbSecurityDescriptor,
  SmbTaskState,
  PoolSlotInfo,
  PoolInfo,
} from './specs/types'

export {
  TaskStatus,
  SmbOperatorKind,
  SlotState,
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
