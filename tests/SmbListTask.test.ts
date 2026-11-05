import { describe, expect, it } from 'vitest';

import { SmbListTask } from '../src/core/SmbListTask';
import {
  SmbError,
  SmbOperatorKind,
  TaskStatus,
  type NativeSmbListTask,
  type SmbFileInfo,
} from '../src/types';

function snapshot(status: TaskStatus, overrides: Record<string, string> = {}): Record<string, string> {
  return {
    exists: '1',
    taskId: 'task-1',
    kind: String(SmbOperatorKind.ListDirectory),
    status: String(status),
    ...overrides,
  };
}

function entry(name: string): SmbFileInfo {
  return {
    name,
    path: `/${name}`,
    size: 0,
    isDirectory: false,
    modifiedAt: 0,
    accessedAt: 0,
    createdAt: 0,
    changedAt: 0,
    childCount: -1,
    children: [],
  };
}

type EntrySubscriber = {
  signal: () => void;
  cursor: number;
  scheduled: boolean;
};

/** Mirrors native EntryStream notify-and-drain delivery. */
class FakeNativeListTask implements NativeSmbListTask {
  current: Record<string, string>;
  readonly listeners = new Map<string, (raw: Record<string, string>) => void>();
  readonly entrySubscribers = new Map<string, EntrySubscriber>();
  readonly unsubscribedEntries: string[] = [];
  readonly emitted: SmbFileInfo[] = [];
  readonly pendingSignals: Array<() => void> = [];
  resultReads = 0;
  private nextId = 1;
  private deferSignals = false;

  constructor(current: Record<string, string>) {
    this.current = current;
  }

  cancel(): void {}

  get(): Record<string, string> {
    return this.current;
  }

  subscribe(listener: (raw: Record<string, string>) => void): string {
    const id = `sub-${this.nextId++}`;
    this.listeners.set(id, listener);
    return id;
  }

  unsubscribe(subscriptionId: string): void {
    this.listeners.delete(subscriptionId);
  }

  getResultValue(): unknown {
    this.resultReads += 1;
    return [...this.emitted];
  }

  subscribeEntries(listener: () => void): string {
    const id = `ent-${this.nextId++}`;
    this.entrySubscribers.set(id, { signal: listener, cursor: 0, scheduled: false });
    return id;
  }

  unsubscribeEntries(subscriptionId: string): void {
    this.unsubscribedEntries.push(subscriptionId);
    this.entrySubscribers.delete(subscriptionId);
  }

  drainEntries(subscriptionId: string): SmbFileInfo[] {
    const sub = this.entrySubscribers.get(subscriptionId);
    if (!sub) return [];
    const out = this.emitted.slice(sub.cursor);
    sub.cursor = this.emitted.length;
    sub.scheduled = false;
    return out;
  }

  /** When true, signals queue until {@link flushSignals}; otherwise they run immediately. */
  setDeferSignals(defer: boolean): void {
    this.deferSignals = defer;
  }

  flushSignals(): void {
    const queued = this.pendingSignals.splice(0);
    for (const signal of queued) signal();
  }

  emitEntries(batch: SmbFileInfo[]): void {
    this.emitted.push(...batch);
    for (const sub of this.entrySubscribers.values()) {
      if (sub.scheduled) continue;
      sub.scheduled = true;
      if (this.deferSignals) this.pendingSignals.push(sub.signal);
      else sub.signal();
    }
  }

  emit(next: Record<string, string>): void {
    this.current = next;
    for (const listener of this.listeners.values()) listener(next);
  }
}

const names = (batches: SmbFileInfo[][]) => batches.map((batch) => batch.map((e) => e.name));

describe('SmbListTask.subscribeEntries', () => {
  it('delivers live batches in order', () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    const task = new SmbListTask('task-1', native);
    const batches: SmbFileInfo[][] = [];

    task.subscribeEntries((batch) => batches.push(batch));
    native.emitEntries([entry('a'), entry('b')]);
    native.emitEntries([entry('c')]);

    expect(names(batches)).toEqual([
      ['a', 'b'],
      ['c'],
    ]);
  });

  it('replays earlier entries to late subscribers', () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    const task = new SmbListTask('task-1', native);
    native.emitEntries([entry('a')]);
    native.emitEntries([entry('b')]);

    const batches: SmbFileInfo[][] = [];
    task.subscribeEntries((batch) => batches.push(batch));
    native.emitEntries([entry('c')]);

    expect(names(batches)).toEqual([
      ['a', 'b'],
      ['c'],
    ]);
  });

  it('fans out to independent subscribers', () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    const task = new SmbListTask('task-1', native);
    const first: SmbFileInfo[][] = [];
    const second: SmbFileInfo[][] = [];

    const unsubscribeFirst = task.subscribeEntries((batch) => first.push(batch));
    task.subscribeEntries((batch) => second.push(batch));
    native.emitEntries([entry('a')]);
    unsubscribeFirst();
    native.emitEntries([entry('b')]);

    expect(names(first)).toEqual([['a']]);
    expect(names(second)).toEqual([['a'], ['b']]);
  });

  it('stops delivery and unsubscribes natively exactly once', () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    const task = new SmbListTask('task-1', native);
    const batches: SmbFileInfo[][] = [];

    const unsubscribe = task.subscribeEntries((batch) => batches.push(batch));
    unsubscribe();
    unsubscribe();
    native.emitEntries([entry('a')]);

    expect(batches).toEqual([]);
    expect(native.unsubscribedEntries).toEqual(['ent-1']);
  });

  it('merges several appends behind one pending signal into a single batch', () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    native.setDeferSignals(true);
    const task = new SmbListTask('task-1', native);
    const batches: SmbFileInfo[][] = [];

    task.subscribeEntries((batch) => batches.push(batch));
    native.emitEntries([entry('a')]);
    native.emitEntries([entry('b')]);
    native.emitEntries([entry('c')]);

    expect(batches).toEqual([]);
    expect(native.pendingSignals).toHaveLength(1);

    native.flushSignals();

    expect(names(batches)).toEqual([['a', 'b', 'c']]);
  });

  it('ignores signals that arrive after unsubscribe', () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    native.setDeferSignals(true);
    const task = new SmbListTask('task-1', native);
    const batches: SmbFileInfo[][] = [];

    const unsubscribe = task.subscribeEntries((batch) => batches.push(batch));
    native.emitEntries([entry('a')]);
    unsubscribe();
    native.flushSignals();

    expect(batches).toEqual([]);
  });

  it('drains the last entries before the terminal status settles', async () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    native.setDeferSignals(true);
    const task = new SmbListTask('task-1', native);
    const batches: SmbFileInfo[][] = [];

    task.subscribeEntries((batch) => batches.push(batch));
    native.emitEntries([entry('a')]);
    native.emitEntries([entry('b')]);

    const done = task.wait();
    native.flushSignals();
    native.emit(snapshot(TaskStatus.Success));

    await expect(done).resolves.toBeUndefined();
    expect(names(batches)).toEqual([['a', 'b']]);
  });
});

describe('SmbListTask completion', () => {
  it('result() resolves with the complete listing', async () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    const task = new SmbListTask('task-1', native);
    const result = task.result();

    native.emitEntries([entry('a')]);
    native.emitEntries([entry('b')]);
    native.emit(snapshot(TaskStatus.Success));

    await expect(result).resolves.toEqual([entry('a'), entry('b')]);
  });

  it('wait() settles without reading the result value', async () => {
    const native = new FakeNativeListTask(snapshot(TaskStatus.Running));
    const task = new SmbListTask('task-1', native);
    const done = task.wait();

    native.emitEntries([entry('a')]);
    native.emit(snapshot(TaskStatus.Success));

    await expect(done).resolves.toBeUndefined();
    expect(native.resultReads).toBe(0);
  });

  it('wait() rejects with the task error', async () => {
    const native = new FakeNativeListTask(
      snapshot(TaskStatus.Error, { errorCode: String(SmbError.AccessDenied), errorMessage: 'denied' }),
    );

    await expect(new SmbListTask('task-1', native).wait()).rejects.toMatchObject({
      code: SmbError.AccessDenied,
      message: 'denied',
    });
  });
});
