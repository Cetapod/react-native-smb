import { describe, expect, it } from 'vitest';

import { SmbTask } from '../src/core/SmbTask';
import { SmbError, SmbTaskError, SmbOperatorKind, TaskStatus, type NativeSmbTask } from '../src/types';

function snapshot(status: TaskStatus, overrides: Record<string, string> = {}): Record<string, string> {
  return {
    exists: '1',
    taskId: 'task-1',
    kind: String(SmbOperatorKind.DownloadFile),
    status: String(status),
    determinate: 'true',
    progress: status === TaskStatus.Success ? '1' : '0.5',
    ...overrides,
  };
}

class FakeNativeTask implements NativeSmbTask {
  current: Record<string, string>;
  readonly listeners = new Map<string, (raw: Record<string, string>) => void>();
  readonly unsubscribed: string[] = [];
  result: unknown = 'result';
  resultError?: Error;
  unsubscribeError?: Error;
  onSubscribe?: (listener: (raw: Record<string, string>) => void) => void;
  getError?: Error;

  constructor(current: Record<string, string>) {
    this.current = current;
  }

  cancel(): void {}

  get(): Record<string, string> {
    if (this.getError) throw this.getError;
    return this.current;
  }

  subscribe(listener: (raw: Record<string, string>) => void): string {
    const id = `sub-${this.listeners.size + 1}`;
    this.listeners.set(id, listener);
    this.onSubscribe?.(listener);
    return id;
  }

  unsubscribe(subscriptionId: string): void {
    this.unsubscribed.push(subscriptionId);
    this.listeners.delete(subscriptionId);
    if (this.unsubscribeError) throw this.unsubscribeError;
  }

  getResultValue(): unknown {
    if (this.resultError) throw this.resultError;
    return this.result;
  }

  emit(next: Record<string, string>): void {
    this.current = next;
    for (const listener of this.listeners.values()) listener(next);
  }
}

describe('SmbTask.result', () => {
  it('settles terminal states read before subscribing without a listener', async () => {
    const success = new FakeNativeTask(snapshot(TaskStatus.Success));
    await expect(new SmbTask<string>('task-1', success).result()).resolves.toBe('result');
    expect(success.listeners.size).toBe(0);

    for (const status of [TaskStatus.Error, TaskStatus.Cancelled]) {
      const native = new FakeNativeTask(
        snapshot(status, { errorCode: String(status === TaskStatus.Cancelled ? SmbError.Cancelled : SmbError.Unknown) }),
      );
      await expect(new SmbTask('task-1', native).result()).rejects.toMatchObject({
        code: status === TaskStatus.Cancelled ? SmbError.Cancelled : SmbError.Unknown,
      } satisfies Partial<SmbTaskError>);
      expect(native.listeners.size).toBe(0);
    }
  });

  it('cleans up when subscription synchronously reports completion', async () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    native.onSubscribe = (listener) => {
      native.current = snapshot(TaskStatus.Success);
      listener(native.current);
    };

    await expect(new SmbTask<string>('task-1', native).result()).resolves.toBe('result');
    expect(native.unsubscribed).toEqual(['sub-1']);
  });

  it('settles only once when terminal events are repeated', async () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    const task = new SmbTask<string>('task-1', native);
    const result = task.result();

    native.emit(snapshot(TaskStatus.Success));
    native.emit(snapshot(TaskStatus.Error));

    await expect(result).resolves.toBe('result');
    expect(native.unsubscribed).toEqual(['sub-1']);
  });

  it('delivers terminal state to every active subscriber', async () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    const task = new SmbTask<string>('task-1', native);
    const observed: TaskStatus[] = [];
    const unsubscribe = task.subscribe((snap) => observed.push(snap.status));
    const result = task.result();

    native.emit(snapshot(TaskStatus.Success));
    native.emit(snapshot(TaskStatus.Error));

    await expect(result).resolves.toBe('result');
    expect(observed).toEqual([TaskStatus.Running, TaskStatus.Success]);
    expect(native.unsubscribed).toEqual(['sub-2']);

    unsubscribe();
    expect(native.unsubscribed).toEqual(['sub-2', 'sub-1']);
  });

  it('rejects and cleans up when result conversion fails', async () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    native.resultError = new Error('result conversion failed');
    const result = new SmbTask<string>('task-1', native).result();

    native.emit(snapshot(TaskStatus.Success));

    await expect(result).rejects.toThrow('result conversion failed');
    expect(native.unsubscribed).toEqual(['sub-1']);
  });

  it('settles even when native unsubscription fails', async () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    native.unsubscribeError = new Error('unsubscribe failed');
    const result = new SmbTask<string>('task-1', native).result();

    native.emit(snapshot(TaskStatus.Success));

    await expect(result).resolves.toBe('result');
    expect(native.unsubscribed).toEqual(['sub-1']);
  });

  it('rejects when native subscription fails', async () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    native.subscribe = () => {
      throw new Error('subscription failed');
    };

    await expect(new SmbTask('task-1', native).result()).rejects.toThrow('subscription failed');
  });

  it('unsubscribes when initial snapshot reading fails after subscription', () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running));
    native.getError = new Error('bridge failure');
    const task = new SmbTask('task-1', native);

    expect(() => task.subscribe()).toThrow('bridge failure');
    expect(native.unsubscribed).toEqual(['sub-1']);
  });

  it('does not regress a cached terminal snapshot', () => {
    const native = new FakeNativeTask(snapshot(TaskStatus.Running, { updatedAt: '1' }));
    const task = new SmbTask('task-1', native);
    const observed: TaskStatus[] = [];
    task.subscribe((snap) => observed.push(snap.status));

    native.emit(snapshot(TaskStatus.Success, { updatedAt: '3' }));
    native.emit(snapshot(TaskStatus.Running, { updatedAt: '2' }));

    expect(task.getSnapshot()?.status).toBe(TaskStatus.Success);
    expect(observed.at(-1)).toBe(TaskStatus.Success);
  });
});
