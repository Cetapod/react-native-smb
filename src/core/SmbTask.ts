import { shapeTaskSnapshot } from '../bridge/decode';
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

  /**
   * Subscribe to live updates. Optional — call only when you need progress UI.
   * Returns unsubscribe. Updates {@link progress} and {@link status} getters.
   */
  subscribe(listener?: (snapshot: SmbTaskState) => void): () => void {
    const subId = this.native.subscribe((raw) => {
      const snap = shapeTaskSnapshot(raw);
      if (!snap) return;
      this.snapshot_ = snap;
      listener?.(snap);
    });

    const initial = this.get();
    if (initial) {
      this.snapshot_ = initial;
      listener?.(initial);
    }

    return () => this.native.unsubscribe(subId);
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
  getRaw(): Record<string, string> {
    return this.native.get();
  }

  /** @internal */
  getResultValue(): T {
    return this.native.getResultValue() as T;
  }
}