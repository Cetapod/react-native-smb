import { isTerminalStatus } from '../labels';
import { SmbError, SmbTaskError, TaskStatus, type SmbTaskState } from '../types';
import type { SmbTask } from './SmbTask';

function errorForSnapshot(snapshot: SmbTaskState, taskId: string): SmbTaskError {
  const message =
    snapshot.errorMessage || (snapshot.status === TaskStatus.Cancelled ? 'Task cancelled' : 'Task failed');
  return new SmbTaskError(message, snapshot.errorCode as SmbError, taskId);
}

/**
 * Subscribe once; on Success resolve with `onSuccess()`, otherwise reject with
 * the task error when the task reaches a terminal status.
 */
function settleTask<T, R>(task: SmbTask<T>, onSuccess: () => R): Promise<R> {
  return new Promise<R>((resolve, reject) => {
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
          resolve(onSuccess());
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

/** Resolve with the typed result, or reject when the task fails or is cancelled. */
export function taskResult<T>(task: SmbTask<T>): Promise<T> {
  return settleTask(task, () => task.getResultValue());
}

/** Like {@link taskResult} without reading (converting) the result value. */
export function taskDone<T>(task: SmbTask<T>): Promise<void> {
  return settleTask(task, () => undefined);
}
