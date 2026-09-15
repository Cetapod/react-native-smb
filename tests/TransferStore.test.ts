import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import type { ReactNativeSmb } from '../src/ReactNativeSmb';
import { TransferStore } from '../src/core/TransferStore';
import { SmbOperatorKind, TaskStatus } from '../src/types';

function snapshot(taskId: string, status: TaskStatus, updatedAt = 1): Record<string, string> {
  return {
    exists: '1',
    taskId,
    kind: String(SmbOperatorKind.DownloadFile),
    status: String(status),
    determinate: 'true',
    progress: status === TaskStatus.Success ? '1' : '0.5',
    updatedAt: String(updatedAt),
  };
}

class FakeNative {
  transfers: Record<string, string>[] = [];
  readonly listeners = new Map<string, (raw: Record<string, string>) => void>();
  readonly unsubscribed: string[] = [];
  throwOnTransfers = false;

  subscribeTaskEvents(listener: (raw: Record<string, string>) => void): string {
    const id = `sub-${this.listeners.size + 1}`;
    this.listeners.set(id, listener);
    return id;
  }

  unsubscribeTaskEvents(id: string): void {
    this.unsubscribed.push(id);
    this.listeners.delete(id);
  }

  getTransferTasks(): Record<string, string>[] {
    if (this.throwOnTransfers) throw new Error('transfer task query failed');
    return this.transfers;
  }

  emit(raw: Record<string, string>): void {
    for (const listener of this.listeners.values()) listener(raw);
  }
}

function nativeBridge(fake: FakeNative): ReactNativeSmb {
  return fake as unknown as ReactNativeSmb;
}

describe('TransferStore', () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => vi.useRealTimers());

  it('keeps an event received before active-task hydration', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Running, 1)];
    const originalSubscribe = native.subscribeTaskEvents.bind(native);
    native.subscribeTaskEvents = (listener) => {
      const id = originalSubscribe(listener);
      listener(snapshot('transfer-1', TaskStatus.Success, 2));
      return id;
    };

    const store = new TransferStore(nativeBridge(native));
    const unsubscribe = store.subscribe(() => {});

    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Success }]);
    unsubscribe();
  });

  it('rolls back a failed start so it can retry', () => {
    const native = new FakeNative();
    native.throwOnTransfers = true;
    const store = new TransferStore(nativeBridge(native));

    expect(() => store.start()).toThrow('transfer task query failed');
    expect(native.unsubscribed).toEqual(['sub-1']);

    native.throwOnTransfers = false;
    native.transfers = [snapshot('transfer-1', TaskStatus.Running)];
    store.start();

    expect(store.getSnapshot()).toHaveLength(1);
  });

  it('keeps terminal snapshots when a delayed running event arrives', () => {
    const native = new FakeNative();
    const store = new TransferStore(nativeBridge(native));
    store.start();

    native.emit(snapshot('transfer-1', TaskStatus.Success, 2));
    native.emit(snapshot('transfer-1', TaskStatus.Running, 1));
    vi.advanceTimersByTime(16);

    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Success }]);
  });

  it('clears local state and ignores late events after stop', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Running)];
    const store = new TransferStore(nativeBridge(native));
    let notifications = 0;
    store.subscribe(() => notifications++);

    store.stop();
    native.emit(snapshot('transfer-1', TaskStatus.Success));
    vi.advanceTimersByTime(16);

    expect(native.unsubscribed).toEqual(['sub-1']);
    expect(store.getSnapshot()).toEqual([]);
    expect(notifications).toBe(1);
  });

  it('ignores a queued callback from a previous subscription after restart', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Running, 1)];
    const store = new TransferStore(nativeBridge(native));
    store.start();
    const oldListener = native.listeners.get('sub-1');

    store.stop();
    store.start();
    oldListener?.(snapshot('transfer-1', TaskStatus.Success, 2));

    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Running }]);
  });

  it('hydrates a terminal transfer that settled before subscription', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Cancelled, 2)];
    const store = new TransferStore(nativeBridge(native));

    store.start();

    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Cancelled }]);
  });

  it('publishes a burst of 180 terminal events once', () => {
    const native = new FakeNative();
    native.transfers = Array.from({ length: 180 }, (_, index) =>
      snapshot(`transfer-${index}`, TaskStatus.Running, 1),
    );
    const store = new TransferStore(nativeBridge(native));
    let notifications = 0;
    store.subscribe(() => notifications++);

    for (let index = 0; index < 180; index++) {
      native.emit(snapshot(`transfer-${index}`, TaskStatus.Cancelled, 2));
    }

    expect(notifications).toBe(0);
    expect(store.getSnapshot().every((task) => task.status === TaskStatus.Running)).toBe(true);

    vi.advanceTimersByTime(16);

    expect(notifications).toBe(1);
    expect(store.getSnapshot()).toHaveLength(180);
    expect(store.getSnapshot().every((task) => task.status === TaskStatus.Cancelled)).toBe(true);
  });

  it('publishes only the latest update for a task in one window', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Pending, 1)];
    const store = new TransferStore(nativeBridge(native));
    let notifications = 0;
    store.subscribe(() => notifications++);

    native.emit(snapshot('transfer-1', TaskStatus.Running, 2));
    native.emit(snapshot('transfer-1', TaskStatus.Cancelled, 3));
    vi.advanceTimersByTime(16);

    expect(notifications).toBe(1);
    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Cancelled }]);
  });

  it('ignores an older non-terminal update', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Pending, 1)];
    const store = new TransferStore(nativeBridge(native));
    store.start();

    native.emit(snapshot('transfer-1', TaskStatus.Running, 3));
    native.emit(snapshot('transfer-1', TaskStatus.Pending, 2));
    vi.advanceTimersByTime(16);

    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Running }]);
  });

  it('cancels a pending publication when stopped', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Running, 1)];
    const store = new TransferStore(nativeBridge(native));
    let notifications = 0;
    store.subscribe(() => notifications++);

    native.emit(snapshot('transfer-1', TaskStatus.Cancelled, 2));
    store.stop();

    expect(notifications).toBe(1);
    expect(store.getSnapshot()).toEqual([]);

    vi.advanceTimersByTime(16);

    expect(notifications).toBe(1);
    expect(store.getSnapshot()).toEqual([]);
  });

  it('publishes an action immediately and absorbs a pending event publication', () => {
    const native = new FakeNative();
    native.transfers = [
      snapshot('transfer-1', TaskStatus.Running, 1),
      snapshot('transfer-2', TaskStatus.Running, 1),
    ];
    const store = new TransferStore(nativeBridge(native));
    let notifications = 0;
    store.subscribe(() => notifications++);

    native.emit(snapshot('transfer-1', TaskStatus.Cancelled, 2));
    store.hide('transfer-2');

    expect(notifications).toBe(1);
    expect(store.getSnapshot()).toMatchObject([{ taskId: 'transfer-1', status: TaskStatus.Cancelled }]);

    vi.advanceTimersByTime(16);

    expect(notifications).toBe(1);
  });

  it('clears a completed task before its pending publication', () => {
    const native = new FakeNative();
    native.transfers = [snapshot('transfer-1', TaskStatus.Running, 1)];
    const store = new TransferStore(nativeBridge(native));
    let notifications = 0;
    store.subscribe(() => notifications++);

    native.emit(snapshot('transfer-1', TaskStatus.Cancelled, 2));
    store.clearCompleted();

    expect(notifications).toBe(1);
    expect(store.getSnapshot()).toEqual([]);

    vi.advanceTimersByTime(16);

    expect(notifications).toBe(1);
  });
});
