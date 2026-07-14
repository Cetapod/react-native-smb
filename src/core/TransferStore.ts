import { shapeTaskSnapshot } from '../bridge/decode';
import { isTransferKind } from '../labels';
import type { ReactNativeSmb } from '../ReactNativeSmb';
import { TaskStatus, type SmbTaskState } from '../types';

type Listener = () => void;

const EMPTY_SNAPSHOT: SmbTaskState[] = [];

export class TransferStore {
  private tasks = new Map<string, SmbTaskState>();
  private hidden = new Set<string>();
  private listeners = new Set<Listener>();
  private subId: string | null = null;
  private started = false;
  private snapshot: SmbTaskState[] = EMPTY_SNAPSHOT;

  constructor(private readonly native: ReactNativeSmb) {}

  start(): void {
    if (this.started) return;
    this.started = true;

    for (const raw of this.native.getActiveTasks()) {
      const snap = shapeTaskSnapshot(raw);
      if (snap && isTransferKind(snap.kind)) {
        this.tasks.set(snap.taskId, snap);
      }
    }
    this.rebuildSnapshot();

    this.subId = this.native.subscribeTaskEvents((raw) => {
      try {
        const snap = shapeTaskSnapshot(raw);
        if (!snap || !isTransferKind(snap.kind)) return;
        if (this.hidden.has(snap.taskId)) return;
        this.tasks.set(snap.taskId, snap);
        this.notify();
      } catch {}
    });
  }

  stop(): void {
    if (this.subId) {
      this.native.unsubscribeTaskEvents(this.subId);
      this.subId = null;
    }
    this.started = false;
  }

  subscribe(listener: Listener): () => void {
    this.start();
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  getSnapshot(): SmbTaskState[] {
    this.start();
    return this.snapshot;
  }

  private rebuildSnapshot(): void {
    if (this.tasks.size === 0) {
      this.snapshot = EMPTY_SNAPSHOT;
      return;
    }
    this.snapshot = Array.from(this.tasks.values())
      .filter((t) => !this.hidden.has(t.taskId))
      .sort((a, b) => b.updatedAt - a.updatedAt);
  }

  clearCompleted(): void {
    for (const [id, snap] of this.tasks) {
      if (
        snap.status === TaskStatus.Success ||
        snap.status === TaskStatus.Error ||
        snap.status === TaskStatus.Cancelled
      ) {
        this.tasks.delete(id);
      }
    }
    this.notify();
  }

  hide(taskId: string): void {
    this.hidden.add(taskId);
    this.tasks.delete(taskId);
    this.notify();
  }

  private notify(): void {
    this.rebuildSnapshot();
    this.listeners.forEach((l) => {
      try {
        l();
      } catch {}
    });
  }
}