import { useCallback, useSyncExternalStore } from 'react';

import type { SmbTask } from '../core/SmbTask';
import { TaskStatus, type SmbTaskState } from '../types';

export interface SubscribeHandle<T = void> {
  /** Start live updates. Returns unsubscribe. */
  subscribe: (listener?: (snapshot: SmbTaskState) => void) => () => void;
  cancel: () => void;
  progress: number;
  status: TaskStatus;
  snapshot: SmbTaskState | null;
  task: SmbTask<T> | null;
}

/**
 * React binding for {@link SmbTask.subscribe}. Subscribes automatically on mount
 * and whenever `task` changes; unsubscribes on unmount / task change.
 *
 * @example
 * const task = smb.downloadFile(remote, local);
 * const { progress, status, cancel } = useSubscribe(task);
 */
export function useSubscribe<T>(task: SmbTask<T> | null | undefined): SubscribeHandle<T> {
  const currentTask = task ?? null;

  const subscribe = useCallback(
    (listener?: (snap: SmbTaskState) => void) => {
      if (!currentTask) return () => {};
      return currentTask.subscribe(listener);
    },
    [currentTask],
  );

  const getSnapshot = useCallback(() => currentTask?.getSnapshot() ?? null, [currentTask]);
  const snapshot = useSyncExternalStore(subscribe, getSnapshot, () => null);

  const cancel = useCallback(() => {
    currentTask?.cancel();
  }, [currentTask]);

  return {
    subscribe,
    cancel,
    progress: snapshot?.progress ?? 0,
    status: snapshot?.status ?? TaskStatus.Pending,
    snapshot,
    task: currentTask,
  };
}
