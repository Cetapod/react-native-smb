import {
  SmbOperatorKind,
  TaskStatus,
  type SmbTaskState,
} from '../../src/types';

export function makeTaskSnapshot(
  status: TaskStatus,
  overrides: Partial<SmbTaskState> = {},
): SmbTaskState {
  return {
    taskId: 'task-1',
    kind: SmbOperatorKind.DownloadFile,
    status,
    determinate: true,
    progress: status === TaskStatus.Success ? 1 : 0.5,
    bytesDone: 0,
    totalBytes: 0,
    bytesPerSecond: 0,
    etaSeconds: 0,
    sourcePath: '',
    destinationPath: '',
    errorCode: 0,
    errorMessage: '',
    startedAt: 0,
    updatedAt: 1,
    endedAt: 0,
    ...overrides,
  };
}
