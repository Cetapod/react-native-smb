import { isTerminalStatus } from '../labels';
import { SmbError, SmbTaskError, TaskStatus, type SmbTaskState } from '../types';
import type { SmbTask } from './SmbTask';

function errorForSnapshot(snapshot: SmbTaskState, taskId: string): SmbTaskError {
  const message =
    snapshot.errorMessage || (snapshot.status === TaskStatus.Cancelled ? 'Task cancelled' : 'Task failed');
  return new SmbTaskError(message, snapshot.errorCode as SmbError, taskId);
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

    const finish = (snapshot: SmbTaskState) => {
      if (settled) return;
      settled = true;
      try {
        if (snapshot.status === TaskStatus.Success) {
          resolve(task.getResultValue());
        } else {
          reject(errorForSnapshot(snapshot, task.id));
        }
      } catch (e) {
        reject(e);
      } finally {
        cleanup();
      }
    };

    try {
      const initial = task.get();
      if (initial && isTerminalStatus(initial.status)) {
        finish(initial);
        return;
      }

      unsubscribe = task.subscribe((snap) => {
        if (isTerminalStatus(snap.status)) finish(snap);
      });

      if (settled) cleanup();
    } catch (e) {
      cleanup();
      reject(e);
    }
  });
}
