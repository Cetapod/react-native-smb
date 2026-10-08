import { shapeTaskSnapshot } from '../bridge/decode';
import { isTerminalStatus } from '../labels';
import { TaskStatus, type NativeSmbTask, type SmbTaskState } from '../types';
import { taskResult } from './taskResult';

/**
 * Handle for one native async operation.
 *
 * Creating a task does not subscribe. Three optional actions:
 * - {@link subscribe} — live progress / status
 * - {@link result} — await completion and get the typed value
 * - {@link cancel} — abort
 */
export class SmbTask<T = void> {
  readonly id: string;
  private snapshot_: SmbTaskState | null = null;

  constructor(
    id: string,
    private readonly native: NativeSmbTask,
  ) {
    this.id = id;
  }

  /** 0–1 after {@link subscribe} has been called. */
  get progress(): number {
    return this.snapshot_?.progress ?? this.get()?.progress ?? 0;
  }

  /** Latest status after {@link subscribe}, or a one-shot read via {@link get}. */
  get status(): TaskStatus {
    return this.snapshot_?.status ?? this.get()?.status ?? TaskStatus.Pending;
  }

  cancel(): void {
    this.native.cancel();
  }

  /** One-shot snapshot without subscribing. */
  get(): SmbTaskState | null {
    return shapeTaskSnapshot(this.native.get());
  }

  /** Latest known snapshot, reading native state once when needed. */
  getSnapshot(): SmbTaskState | null {
    if (!this.snapshot_) this.snapshot_ = this.get();
    return this.snapshot_;
  }

  private applySnapshot(next: SmbTaskState): SmbTaskState | null {
    const current = this.snapshot_;
    if (current && isTerminalStatus(current.status)) return current;
    if (current && next.updatedAt < current.updatedAt) return null;
    this.snapshot_ = next;
    return next;
  }

  /**
   * Subscribe to live updates. Optional — call only when you need progress UI.
   * Returns unsubscribe. Updates {@link progress} and {@link status} getters.
   */
  subscribe(listener?: (snapshot: SmbTaskState) => void): () => void {
    let active = true;
    let subId: string | null = null;
    let terminalDelivered = false;

    const receiveSnapshot = (snap: SmbTaskState) => {
      if (!active) return;
      const effective = this.applySnapshot(snap);
      if (!effective) return;
      if (isTerminalStatus(effective.status)) {
        if (terminalDelivered) return;
        terminalDelivered = true;
      }
      listener?.(effective);
    };

    const receive = (raw: Record<string, string>) => {
      const snap = shapeTaskSnapshot(raw);
      if (snap) receiveSnapshot(snap);
    };

    const unsubscribe = () => {
      if (!active) return;
      active = false;
      try {
        if (subId) this.native.unsubscribe(subId);
      } catch {}
    };

    try {
      subId = this.native.subscribe(receive);
      const initial = this.get();
      if (initial) receiveSnapshot(initial);
    } catch (error) {
      unsubscribe();
      throw error;
    }

    return unsubscribe;
  }

  /**
   * Await the typed result. Subscribes only when called — not at task creation.
   *
   * @example
   * const task = smb.listDirectory('/', false, -1);
   * const files = await task.result();
   */
  result(): Promise<T> {
    return taskResult(this);
  }

  /** Like `Promise.allSettled` over {@link result}. */
  static settleAll<T>(tasks: SmbTask<T>[]): Promise<PromiseSettledResult<T>[]> {
    return Promise.allSettled(tasks.map((task) => task.result()));
  }

  /** @internal */
  getResultValue(): T {
    return this.native.getResultValue() as T;
  }
}
