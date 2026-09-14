# Task model

Every SMB operation returns a `SmbTask<T>`. Creating the task starts native work immediately. Nothing is subscribed until you ask for it.

You can mix three actions on any task:

| Action | When | API |
| ------ | ---- | --- |
| Get result | You need the return value | `await task.result()` |
| Watch progress | UI needs updates | `task.subscribe()` or `useSubscribe` |
| Cancel | User aborts | `task.cancel()` |

## Get the result

```typescript
import { SMB, type SmbCredentials } from '@cetapod/react-native-smb';

const smb = SMB();
const creds: SmbCredentials = { username: 'user', password: 'pass' };

await smb.connect('smb://192.168.1.100/Share', creds).result();

const files = await smb.listDirectory('/', false, -1).result();
await smb.downloadFile('/doc.pdf', '/local/doc.pdf').result();
```

Hold the handle and await later:

```typescript
const task = smb.copyItem('/src', '/dst', true);
await task.result();
```

Run several tasks and collect outcomes (like `Promise.allSettled`):

```typescript
import { SmbTask } from '@cetapod/react-native-smb';

const tasks = files.map((f) => smb.deleteItem(f.path));
const results = await SmbTask.settleAll(tasks);
```

## Watch progress

Imperative:

```typescript
const task = smb.downloadFile(remote, local);

const unsub = task.subscribe(); // live task.progress / task.status
// or:
const unsub2 = task.subscribe((snap) => {
  console.log(snap.progress, snap.bytesPerSecond, snap.status);
});

task.cancel();
unsub();
```

React — `useSubscribe` subscribes automatically on mount and unsubscribes on unmount / task change:

```tsx
import { useSubscribe, statusLabel } from '@cetapod/react-native-smb';

function DownloadBar({ task }) {
  const { progress, status, cancel } = useSubscribe(task);

  return (
    <>
      <Text>{Math.round(progress * 100)}%</Text>
      <Text>{statusLabel(status)}</Text>
      <Button onPress={cancel} title="Cancel" />
    </>
  );
}
```

`progress` and `status` on the task object are live getters. Don't destructure them at creation time — read them after subscribing (`task.subscribe()`), or use `useSubscribe`, which subscribes for you.

### Snapshot fields

`subscribe` and `get()` return `SmbTaskState`:

| Field | Description |
| ----- | ----------- |
| `progress` | `0–1` (determinate for transfers) |
| `status` | `TaskStatus` enum |
| `bytesDone` / `totalBytes` | Raw byte counts |
| `bytesPerSecond` / `etaSeconds` | Throughput estimate |
| `determinate` | `true` for download/upload/copy |
| `sourcePath` / `destinationPath` | Paths for transfer ops |
| `errorCode` / `errorMessage` | Set on failure |

## Fire-and-forget

Start work without awaiting — useful for bulk ops with a transfer tray:

```typescript
files.forEach((f) => smb.downloadFile(f.remote, f.local));
```

Refresh when everything settles:

```typescript
const tasks = files.map((f) => smb.deleteItem(f.path));
void SmbTask.settleAll(tasks).then(() => reload());
```

### Transfer tray hooks

```typescript
import { useTransfers, useTransferActions } from '@cetapod/react-native-smb';

function TransferTray({ smb }) {
  const tasks = useTransfers(smb);
  const { clearCompleted, hide } = useTransferActions(smb);

  // tasks is SmbTaskState[] — all active transfers in the client store
}
```

The example app's `TransferTrayPanel` shows this pattern end-to-end.

## Error handling

```typescript
import { SMB, SmbError, SmbTaskError } from '@cetapod/react-native-smb';

try {
  await smb.getPathInfo('/nonexistent').result();
} catch (error) {
  if (error instanceof SmbTaskError) {
    if (error.code === SmbError.NotFound) { /* ... */ }
    if (error.code === SmbError.AccessDenied) { /* ... */ }
    if (error.code === SmbError.AuthenticationFailed) { /* prompt for credentials */ }
    if (error.code === SmbError.NotConnected || error.code === SmbError.TimedOut) { /* reconnect/retry */ }
  }
}
```

`SmbTaskError` carries `code` (`SmbError` enum) and optional `taskId`.

The task preserves the first causal operation error. Cancellation is returned only when cancellation was observed before another failure. Recursive operations fail rather than silently returning partial results.

## Lifecycle

To stop transfer tray subscriptions without tearing down the SMB session:

```typescript
smb.transferStore().stop();
```

When you're done with a client instance (e.g. user logs out), shut it down completely:

```typescript
await smb.destroy();
```

Create a new client with `SMB()` afterwards — the instance is not reusable.

Sync state checks (`isConnected`, `isInitialized`) are available anytime without a task.
