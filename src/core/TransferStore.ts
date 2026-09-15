import { shapeTaskSnapshot } from '../bridge/decode';
import { isTransferKind } from '../labels';
import type { ReactNativeSmb } from '../ReactNativeSmb';
import { TaskStatus, type SmbTaskState } from '../types';

type Listener = () => void;

const EMPTY_SNAPSHOT: SmbTaskState[] = [];
const PUBLISH_INTERVAL_MS = 16;

function isTerminal(status: TaskStatus): boolean {
  return status === TaskStatus.Success || status === TaskStatus.Error || status === TaskStatus.Cancelled;
}

export class TransferStore {
  private tasks = new Map<string, SmbTaskState>();
  private hidden = new Set<string>();
  private listeners = new Set<Listener>();
  private subId: string | null = null;
  private started = false;
  private generation = 0;
  private snapshot: SmbTaskState[] = EMPTY_SNAPSHOT;
  private publishTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(private readonly native: ReactNativeSmb) {}

  start(): void {
    if (this.started) return;
    let subId: string | null = null;
    const generation = ++this.generation;

    try {
      subId = this.native.subscribeTaskEvents((raw) => {
        if (this.generation !== generation) return;
        try {
          this.upsert(raw);
        } catch {}
      });

      for (const raw of this.native.getTransferTasks()) {
        const snap = shapeTaskSnapshot(raw);
        if (!snap || !isTransferKind(snap.kind) || this.hidden.has(snap.taskId)) continue;
        // Events received after subscription are newer than this hydration snapshot.
        if (!this.tasks.has(snap.taskId)) this.tasks.set(snap.taskId, snap);
      }

      this.subId = subId;
      this.started = true;
      this.publishNow();
    } catch (error) {
      if (this.generation === generation) this.generation++;
      try {
        if (subId) this.native.unsubscribeTaskEvents(subId);
      } catch {}
      this.tasks.clear();
      this.publishNow();
      throw error;
    }
  }

  stop(): void {
    this.generation++;
    if (this.subId) {
      try {
        this.native.unsubscribeTaskEvents(this.subId);
      } catch {}
      this.subId = null;
    }
    this.started = false;
    this.tasks.clear();
    this.hidden.clear();
    this.publishNow();
  }

  subscribe(listener: Listener): () => void {
    this.start();
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  getSnapshot(): SmbTaskState[] {
    return this.snapshot;
  }

  private upsert(raw: Record<string, string>): void {
    const snap = shapeTaskSnapshot(raw);
    if (!snap || !isTransferKind(snap.kind) || this.hidden.has(snap.taskId)) return;

    const existing = this.tasks.get(snap.taskId);
    if (existing && (isTerminal(existing.status) || snap.updatedAt < existing.updatedAt)) return;

    this.tasks.set(snap.taskId, snap);
    this.schedulePublish();
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
    this.publishNow();
  }

  hide(taskId: string): void {
    this.hidden.add(taskId);
    this.tasks.delete(taskId);
    this.publishNow();
  }

  private schedulePublish(): void {
    if (this.publishTimer !== null) return;
    const generation = this.generation;
    this.publishTimer = setTimeout(() => {
      this.publishTimer = null;
      if (this.generation !== generation) return;
      this.publish();
    }, PUBLISH_INTERVAL_MS);
  }

  private publishNow(): void {
    if (this.publishTimer !== null) {
      clearTimeout(this.publishTimer);
      this.publishTimer = null;
    }
    this.publish();
  }

  private publish(): void {
    this.rebuildSnapshot();
    this.listeners.forEach((l) => {
      try {
        l();
      } catch {}
    });
  }
}
