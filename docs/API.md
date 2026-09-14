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
| `getPathInfo(path)` | `SmbTask<SmbFileInfo>` | `SmbFileInfo` | Includes SD when available |
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

`getPathInfo` always attempts the SD query for its item. If that query fails, path metadata is still returned without `securityDescriptor`.

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
| `destroy()` | `Promise<void>` | Cancel tasks, disconnect pool, stop transfer tracking; not reusable |

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

| Code | `SmbError` | Meaning |
| ---- | ---------- | ------- |
| 0 | `Unknown` | An unexpected internal or malformed bridge error. SMB operation failures use a specific code. |
| 1 | `Cancelled` | The caller cancelled before the operation failed. |
| 100 | `InvalidArgument` | Invalid URL, path, or operation argument. |
| 101 | `NotFound` | The requested remote item does not exist. |
| 102 | `AlreadyExists` | A destination or duplicate name is already present. |
| 103 | `AccessDenied` | The authenticated account lacks access to the item or share. |
| 104 | `NotDirectory` | A directory operation targeted a file. |
| 105 | `IsDirectory` | A file operation targeted a directory. |
| 106 | `DirectoryNotEmpty` | A directory could not be removed because it contains items. |
| 200 | `NotConnected` | The SMB transport closed or is unavailable. |
| 201 | `ConnectionRefused` | The server refused the connection. |
| 202 | `TimedOut` | The SMB operation made no progress before its timeout. |
| 203 | `AuthenticationFailed` | Credentials or account state prevented authentication. |
| 300 | `Busy` | The item is locked or otherwise busy. |
| 301 | `NoSpace` | Local or remote storage is full. |
| 302 | `Io` | Other local or remote I/O failure. |

Recursive listing, copy, duplicate, and delete fail on the first descendant failure; they never return a silent partial success. Security descriptors are omitted only when the account is denied descriptor access.

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
