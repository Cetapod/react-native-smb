import {
  SmbOperatorKind,
  SmbError,
  SlotState,
  TaskStatus,
  type PoolInfo,
  type PoolSlotInfo,
  type SmbTaskState,
} from '../types';

function parseIntField(raw: Record<string, string>, key: string): number {
  const v = raw[key];
  if (v === undefined || v === '') {
    throw new Error(`Missing bridge field: ${key}`);
  }
  const n = parseInt(v, 10);
  if (!Number.isFinite(n)) {
    throw new Error(`Invalid bridge field ${key}: ${v}`);
  }
  return n;
}

function parseIntOptional(raw: Record<string, string>, key: string, fallback = 0): number {
  const v = raw[key];
  if (v === undefined || v === '') return fallback;
  const n = parseInt(v, 10);
  return Number.isFinite(n) ? n : fallback;
}

function parseFloatField(raw: Record<string, string>, key: string, fallback = 0): number {
  const v = raw[key];
  if (v === undefined || v === '') return fallback;
  const n = parseFloat(v);
  return Number.isFinite(n) ? n : fallback;
}

function parseBoolField(raw: Record<string, string>, key: string): boolean {
  const v = raw[key];
  return v === '1' || v === 'true';
}

function enumFromInt<T extends number>(value: number, enumObj: Record<string, number | string>): T {
  const valid = Object.values(enumObj).filter((v): v is number => typeof v === 'number');
  if (valid.includes(value)) {
    return value as T;
  }
  throw new Error(`Invalid enum value: ${value}`);
}

function errorFromInt(value: number): SmbError {
  const valid = Object.values(SmbError).filter((v): v is SmbError => typeof v === 'number');
  return valid.includes(value as SmbError) ? (value as SmbError) : SmbError.Unknown;
}

export function shapeTaskSnapshot(raw: Record<string, string>): SmbTaskState | null {
  if (raw.exists === '0') return null;

  const kind = enumFromInt(parseIntField(raw, 'kind'), SmbOperatorKind);
  const status = enumFromInt(parseIntField(raw, 'status'), TaskStatus);

  return {
    taskId: raw.taskId ?? '',
    kind,
    status,
    determinate: parseBoolField(raw, 'determinate'),
    progress: parseFloatField(raw, 'progress'),
    bytesDone: parseIntOptional(raw, 'bytesDone'),
    totalBytes: parseIntOptional(raw, 'totalBytes'),
    bytesPerSecond: parseFloatField(raw, 'bytesPerSecond'),
    etaSeconds: parseFloatField(raw, 'etaSeconds'),
    sourcePath: raw.sourcePath ?? '',
    destinationPath: raw.destinationPath ?? '',
    errorCode: errorFromInt(parseIntOptional(raw, 'errorCode')),
    errorMessage: raw.errorMessage ?? '',
    startedAt: parseIntOptional(raw, 'startedAt'),
    updatedAt: parseIntOptional(raw, 'updatedAt'),
    endedAt: parseIntOptional(raw, 'endedAt'),
  };
}

function shapePoolSlot(raw: Record<string, string>): PoolSlotInfo {
  return {
    index: parseIntField(raw, 'index'),
    interactiveOnly: parseBoolField(raw, 'interactiveOnly'),
    state: enumFromInt(parseIntField(raw, 'state'), SlotState),
    isConnected: parseBoolField(raw, 'isConnected'),
    shareName: raw.shareName ?? '',
    taskId: raw.taskId ?? '',
    kind: enumFromInt(parseIntField(raw, 'kind'), SmbOperatorKind),
  };
}

export function shapePoolInfo(raw: Array<Record<string, string>>): PoolInfo {
  if (!raw || raw.length === 0) return { poolSize: 0, slots: [] };
  const poolSize = parseIntField(raw[0], 'poolSize');
  return {
    poolSize,
    slots: raw.map(shapePoolSlot),
  };
}

export function terminalStatus(raw: Record<string, string>): TaskStatus | null {
  if (raw.exists === '0') return null;
  const status = enumFromInt(parseIntField(raw, 'status'), TaskStatus);
  if (
    status === TaskStatus.Success ||
    status === TaskStatus.Error ||
    status === TaskStatus.Cancelled
  ) {
    return status;
  }
  return null;
}
