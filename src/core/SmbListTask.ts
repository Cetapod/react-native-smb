import type { NativeSmbListTask, SmbFileInfo } from '../types';
import { SmbTask } from './SmbTask';

/**
 * Task returned by `listDirectory`. Top-level entries are delivered in batches
 * while the listing is still running; `result()` still resolves with the
 * complete list.
 */
export class SmbListTask extends SmbTask<SmbFileInfo[]> {
  constructor(
    id: string,
    private readonly nativeList: NativeSmbListTask,
  ) {
    super(id, nativeList);
  }

  /**
   * Receive top-level entries as they become available, in server order. The
   * first call delivers every entry emitted so far, so subscribing late loses
   * nothing. Each entry is final: directories carry `childCount`, and with a
   * non-zero `maxDepth` their complete `children` subtree.
   *
   * Entries received before a failure or cancellation are an incomplete listing.
   *
   * @example
   * const task = smb.listDirectory('/Media');
   * const unsubscribe = task.subscribeEntries((batch) => append(batch));
   * await task.wait();
   * unsubscribe();
   */
  subscribeEntries(listener: (batch: SmbFileInfo[]) => void): () => void {
    let active = true;
    const subscriptionId = this.nativeList.subscribeEntries((batch) => {
      if (active) listener(batch);
    });

    return () => {
      if (!active) return;
      active = false;
      try {
        this.nativeList.unsubscribeEntries(subscriptionId);
      } catch {}
    };
  }
}
