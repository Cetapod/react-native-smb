import { useCallback, useEffect, useState } from 'react';

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
  const [snapshot, setSnapshot] = useState<SmbTaskState | null>(() => task?.get() ?? null);

  const subscribe = useCallback(
    (listener?: (snap: SmbTaskState) => void) => {
      if (!task) return () => {};
      return task.subscribe((snap) => {
        setSnapshot(snap);
        listener?.(snap);
      });
    },
    [task],
  );

  useEffect(() => {
    setSnapshot(task?.get() ?? null);
    return subscribe();
  }, [task, subscribe]);

  const cancel = useCallback(() => {
    task?.cancel();
  }, [task]);

  return {
    subscribe,
    cancel,
    progress: snapshot?.progress ?? task?.progress ?? 0,
    status: snapshot?.status ?? task?.status ?? TaskStatus.Pending,
    snapshot,
    task: task ?? null,
  };
}