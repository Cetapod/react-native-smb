import { terminalStatus } from '../bridge/decode';
import { SmbError, SmbTaskError, TaskStatus } from '../types';
import type { SmbTask } from './SmbTask';

function errorForStatus(
  status: TaskStatus,
  raw: Record<string, string>,
  taskId: string,
): SmbTaskError {
  const parsed = parseInt(raw.errorCode ?? '0', 10);
  const code = Object.values(SmbError).includes(parsed as SmbError) ? (parsed as SmbError) : SmbError.Unknown;
  const message =
    raw.errorMessage || (status === TaskStatus.Cancelled ? 'Task cancelled' : 'Task failed');
  return new SmbTaskError(message, code, taskId);
}

/** Subscribe once, resolve or reject when the task reaches a terminal status. */
export function taskResult<T>(task: SmbTask<T>): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    let settled = false;
    let unsubscribe = () => {};

    const cleanup = () => {
      try {
        unsubscribe();
      } catch {}
    };

    const finish = (status: TaskStatus, raw: Record<string, string>) => {
      if (settled) return;
      settled = true;
      try {
        if (status === TaskStatus.Success) {
          resolve(task.getResultValue());
        } else {
          reject(errorForStatus(status, raw, task.id));
        }
      } catch (e) {
        reject(e);
      } finally {
        cleanup();
      }
    };

    try {
      const raw = task.getRaw();
      const status = terminalStatus(raw);
      if (status !== null) {
        finish(status, raw);
        return;
      }

      unsubscribe = task.subscribe((snap) => {
        if (
          snap.status === TaskStatus.Success ||
          snap.status === TaskStatus.Error ||
          snap.status === TaskStatus.Cancelled
        ) {
          finish(snap.status, {
            errorCode: String(snap.errorCode),
            errorMessage: snap.errorMessage,
          });
        }
      });

      if (settled) cleanup();
    } catch (e) {
      cleanup();
      reject(e);
    }
  });
}
