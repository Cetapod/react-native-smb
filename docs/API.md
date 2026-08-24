# API reference

All async methods return `SmbTask<T>`. Await with `.result()`. See [Task model](./TASK_MODEL.md) for subscribe, cancel, and hook patterns.

## Connection

| Method | Returns | `result()` type | Notes |
| ------ | ------- | --------------- | ----- |
| `initialize(url, creds)` | `SmbTask<void>` | `void` | Server only, no share |
| `connect(url, creds)` | `SmbTask<SmbConnectionInfo>` | `SmbConnectionInfo` | Auth + mount share from URL |
| `connectShare(share)` | `SmbTask<SmbConnectionInfo>` | `SmbConnectionInfo` | Switch share |
| `disconnect()` | `SmbTask<void>` | `void` | Close session |
| `listShares()` | `SmbTask<SmbShareList[]>` | `SmbShareList[]` | Enumerate shares |
| `isConnected()` | `boolean` | — | Sync |
| `isInitialized()` | `boolean` | — | Sync |

URL: `smb://[<domain;][<username>@]<host>[:<port>]/<share>/<path>`

```typescript
interface SmbCredentials { username: string; password: string; }

interface SmbConnectionInfo {
  url: string; server: string; share: string; isConnected: boolean;
}

interface SmbShareList { name: string; comment: string; }
```

## File system

| Method | Returns | `result()` type | Notes |
| ------ | ------- | --------------- | ----- |
| `listDirectory(path, recursive?, maxDepth?, includeSecurityDescriptor?)` | `SmbTask<SmbFileInfo[]>` | `SmbFileInfo[]` | `maxDepth: -1` = unlimited; SD defaults to `false` |
| `getPathInfo(path)` | `SmbTask<SmbFileInfo>` | `SmbFileInfo` | |
| `downloadFile(remote, local)` | `SmbTask<void>` | `void` | `subscribe()` for progress |
| `uploadFile(local, remote)` | `SmbTask<void>` | `void` | `subscribe()` for progress |
| `createDirectory(path)` | `SmbTask<void>` | `void` | Creates parents |
| `deleteItem(path)` | `SmbTask<void>` | `void` | Recursive for dirs |
| `renameItem(path, newName)` | `SmbTask<void>` | `void` | Basename only |
| `moveItem(from, to)` | `SmbTask<void>` | `void` | |
| `copyItem(from, to, recursive?)` | `SmbTask<void>` | `void` | |
| `duplicateItem(path)` | `SmbTask<string>` | `string` | New path |
| `getSecurityDescriptor(path)` | `SmbTask<SmbSecurityDescriptor>` | `SmbSecurityDescriptor` | |

```typescript
interface SmbFileInfo {
  name: string; path: string; size: number; isDirectory: boolean;
  modifiedAt: number; accessedAt: number; createdAt: number; changedAt: number;
  childCount?: number; children?: SmbFileInfo[];
  securityDescriptor?: SmbSecurityDescriptor;
}
```

Set `includeSecurityDescriptor` to `true` to issue an additional SD query for each returned item, including recursive children. If an item's SD query fails, the item remains in the result without `securityDescriptor`.

```typescript
const files = await smb.listDirectory('/', false, -1, true).result();
const firstDacl = files[0]?.securityDescriptor?.dacl;
```

## Pool

| Method | Returns | Notes |
| ------ | ------- | ----- |
| `getPoolInfo()` | `PoolInfo` | Slot snapshot |
| `subscribePoolInfo(cb)` | `string` | Listener ID |
| `unsubscribePoolInfo(id)` | `void` | |
| `resetPool()` | `void` | Disconnect all slots |
| `dispose()` | `void` | Tear down transfer store |

See [Connection pool](./ARCHITECTURE.md).

## Types

### `SmbTask<T>`

| Member | Description |
| ------ | ----------- |
| `id` | Task identifier |
| `progress`, `status` | Live getters after `subscribe()` |
| `result()` | `Promise<T>` |
| `subscribe(listener?)` | Returns unsubscribe |
| `cancel()` | Abort |
| `get()` | One-shot snapshot |
| `SmbTask.settleAll(tasks)` | `Promise.allSettled` over results |

### Errors

`SmbTaskError` with `code: SmbError` and optional `taskId`.

`SmbError`: `Unknown`, `Cancelled`, `InvalidArgument`, `NotFound`, `AlreadyExists`, `AccessDenied`, `NotDirectory`, `IsDirectory`, `DirectoryNotEmpty`, `NotConnected`, `ConnectionRefused`, `TimedOut`, `Busy`, `NoSpace`, `Io`.

### Hooks

| Hook | Purpose |
| ---- | ------- |
| `useSubscribe(task)` | React state for progress/status |
| `useTransfers(smb)` | Active transfer tasks |
| `useTransferActions(smb)` | `clearCompleted()`, `hide(taskId)` |

## Utilities

Path helpers:

| Function | Description |
| -------- | ----------- |
| `normalizePath` | Forward slashes, trim trailing `/` |
| `splitPath` | `{ parentDirectory, baseFileName, extension, fullFileName }` |
| `joinPath`, `getParentPath`, `getFileName` | Path composition |
| `generateCopyName` | `"file copy 2.pdf"` naming |
| `formatFileSize`, `formatDate` | Display helpers |

Label helpers: `statusLabel`, `operationLabel`, `isTransferKind`, `isDeterminateKind`, `isTerminalStatus`.
