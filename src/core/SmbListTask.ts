import type { NativeSmbListTask, SmbFileInfo } from '../types';
import { SmbTask } from './SmbTask';

/**
 * Task returned by `listDirectory`. Top-level entries are delivered in batches
 * while the listing is still running; `result()` still resolves with the
 * complete list.
 *
 * Native flushes wake a payload-free signal; this wrapper drains coalesced
 * entries so a busy JS thread receives fewer, larger batches.
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
   * nothing. While the JS thread is busy, native flushes coalesce into one
   * batch per wake. Each entry is final: directories carry `childCount`, and
   * with a non-zero `maxDepth` their complete `children` subtree.
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
    let subscriptionId: string | null = null;

    const deliver = () => {
      if (!active || subscriptionId == null) return;
      const batch = this.nativeList.drainEntries(subscriptionId);
      if (batch.length > 0) listener(batch);
    };

    subscriptionId = this.nativeList.subscribeEntries(deliver);
    deliver();

    return () => {
      if (!active) return;
      active = false;
      try {
        this.nativeList.unsubscribeEntries(subscriptionId!);
      } catch {}
    };
  }
}
