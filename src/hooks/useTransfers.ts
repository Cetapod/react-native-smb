import { useEffect, useMemo, useState } from 'react';
import type { Smb } from '../core/SMB';
import type { SmbClient } from '../core/SmbClient';
import type { SmbTaskState } from '../types';

const EMPTY_TASKS: SmbTaskState[] = [];

function store(smb: Smb) {
  return (smb as SmbClient).transferStore();
}

export function useTransfers(smb: Smb | null): SmbTaskState[] {
  const [tasks, setTasks] = useState<SmbTaskState[]>(EMPTY_TASKS);

  useEffect(() => {
    if (!smb) {
      setTasks(EMPTY_TASKS);
      return undefined;
    }
    const transferStore = store(smb);
    const onStoreChange = () => {
      setTasks(transferStore.getSnapshot());
    };
    const unsubscribe = transferStore.subscribe(onStoreChange);
    onStoreChange();
    return unsubscribe;
  }, [smb]);

  return smb ? tasks : EMPTY_TASKS;
}

export function useTransferActions(smb: Smb | null) {
  const s = smb ? store(smb) : null;
  return useMemo(
    () => ({
      clearCompleted: () => s?.clearCompleted(),
      hide: (taskId: string) => s?.hide(taskId),
    }),
    [s],
  );
}