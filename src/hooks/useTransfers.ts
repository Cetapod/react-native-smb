import { useCallback, useMemo, useSyncExternalStore } from 'react';
import type { Smb } from '../core/SMB';
import type { SmbClient } from '../core/SmbClient';
import type { SmbTaskState } from '../types';

const EMPTY_TASKS: SmbTaskState[] = [];

function store(smb: Smb) {
  return (smb as SmbClient).transferStore();
}

export function useTransfers(smb: Smb | null): SmbTaskState[] {
  const transferStore = smb ? store(smb) : null;
  const subscribe = useCallback(
    (listener: () => void) => transferStore?.subscribe(listener) ?? (() => {}),
    [transferStore],
  );
  const getSnapshot = useCallback(() => transferStore?.getSnapshot() ?? EMPTY_TASKS, [transferStore]);

  return useSyncExternalStore(subscribe, getSnapshot, () => EMPTY_TASKS);
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
