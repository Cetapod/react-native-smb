# Connection pool

`HybridSMB` runs up to four SMB operations in parallel through a `SmbConnectionPool(4)`. Each slot is one `SmbConnectionManager` / `smb2_context` — its own TCP socket and session:

```
TCP → NEGOTIATE → SESSION SETUP → TREE CONNECT
```

Each task is granted one slot before its operator runs, holds it for the operator's lifetime, then releases it via `PoolContextHandle`. One operator per slot (mutex on the context). Work on different slots runs concurrently.

From `SmbConnectionPool.hpp`:

```
slot[0]       — Interactive-only
slot[1..N-1]  — General (any AcquireMode; Interactive may overflow here)
```

| Slot | Accepts | Typical work |
| ---- | ------- | ------------ |
| `slot[0]` | `Interactive` only | `listDirectory`, `getPathInfo`, rename, move |
| `slot[1..3]` | Any | Download, upload, copy, delete; grows on demand |

Metadata never uses slot 0. Interactive work can overflow to slots 1–3 when slot 0 is busy.

### Acquire modes

| Mode | Operators |
| ---- | --------- |
| `Interactive` | `getPathInfo`, `listDirectory` (`maxDepth: 0`), `moveItem`, `renameItem`, `createDirectory`, `getSecurityDescriptor`, `listShares` |
| `Metadata` | `downloadFile`, `uploadFile`, `copyItem`, `duplicateItem`, `deleteItem`, `listDirectory` (`maxDepth != 0`) |

FIFO queue per mode; queued Interactive requests are assigned before Metadata. Every request may open a new slot up to the pool size.

### Task admission

The pool queue is the only place a work task waits. Starting a task does not block the calling thread or use Nitro's `Promise::async` pool:

```
JS call → prepare()              validate arguments, record paths (no SMB)
        → requestContext(mode)   queue; status Pending
        → slot reserved          start one thread for the task
        → activate()             connect the slot if needed, on that thread
        → run()                  status Running; slot released when it returns
```

- A queued task holds no thread. At most one thread per pool slot runs work, plus the lifecycle thread.
- A task acquires exactly one slot, once, before `run()`; operators never request another one.
- `initialize`, `connect`, `connectShare`, and `disconnect` take no slot. They run one at a time on a dedicated lifecycle thread, so they never wait behind queued work; a transition cancels queued and running tasks before it changes the connection.
- Cancelling a queued task settles it at once without starting a thread. Grant and rejection callbacks run outside the pool lock.

The server sees each active slot as a separate client connection — multiple full sessions to the same host/share, not channels bound to one session. SMB3 multichannel is the protocol-level alternative: extra TCP links attach to an existing session via `SMB2_SESSION_FLAG_BINDING` and share one `SessionId`. libsmb2 does not implement that; each `smb2_context` maps to one `fd`, so the pool uses independent contexts instead.

| | Pool | SMB3 multichannel |
| --- | --- | --- |
| Sessions | One per slot | One across channels |
| Server view | N connections | 1 client, N channels |
| Slot 0 reserved for UI | Yes | — |
| Requires server multichannel cap | No | Yes |

Slot 0 is application-level scheduling on top of what libsmb2 provides — interactive ops stay off the slots running bulk transfers.

## Chunk pipeline

Cross-task parallelism is the pool. Within a single transfer, `SmbFileIO` pipelines up to eight SMB2 READ/WRITE on the same `smb2_context` (`kMaxInFlight` in `SmbFileIO.cpp`). Same connection, same session — credit-based pipelining, separate from slot assignment.

## Server-side copy

`copyItem` and `duplicateItem` copy file data inside the server, so it never crosses the client's network link:

```
open source, create destination
→ FSCTL_SRV_REQUEST_RESUME_KEY on the source        24-byte key
→ FSCTL_SRV_COPYCHUNK_WRITE on the destination       up to 4 in flight, 16 × 1 MiB chunks each
→ fsync, close
```

- Each request stays within the protocol's default server limits (256 chunks, 1 MiB each, 16 MiB total). Progress advances per completed request.
- If the server rejects the copy before any data has moved (`NOT_SUPPORTED`, `INVALID_DEVICE_REQUEST`, `NOT_IMPLEMENTED`, `INVALID_PARAMETER`), the file is copied through the client with pipelined READ/WRITE instead. The rejection is remembered in `ServerCapabilities`, shared by every pool slot and replaced when the server or share changes, so it is probed and logged once.
- A failure after data started moving fails the copy; the partial destination is deleted, as with the client copy.
- Server-side copy only works within one share, which every pool slot uses.

## Pipelined tree operations

Recursive delete and copy send one small request per file or folder. `PipelinedOps` keeps several of them in flight on one held slot; each completion may submit follow-up requests, which expresses the ordering. The first failure or a cancel stops new requests, drains the ones in flight, and still runs cleanup (closing handles, removing partial files).

| Operation | In flight | Ordering |
|-----------|-----------|----------|
| `deleteItem` | 16 requests | files and empty folders at once; a folder once its contents are gone; the root folder last |
| Copy: folders | 16 requests | a folder once its parent exists |
| Copy: files | 8 files | each file runs open → stat → resume key → COPYCHUNK_WRITE → fsync → close as its own chain |

A recursive copy sends its first non-empty file alone to find out whether the server supports server-side copy. If it does, the remaining files are copied 8 at a time; if not, they are copied through the client one after another. Files in progress when a copy fails or is cancelled are removed; finished files stay.

## Listing pipeline

`listDirectory` runs `DirectoryLister` on one slot — Interactive for `maxDepth: 0`, Metadata for nested listings — instead of libsmb2's `smb2_opendir` (which reads the whole directory before returning):

```
[CREATE + QUERY_DIRECTORY] → QUERY_DIRECTORY … → CLOSE       decode each reply as it arrives
   └ directory within maxDepth: same chain, decoded          expand into children
   └ directory at the limit:    same chain, counted          childCount
   └ per entry (opt-in):        CREATE + QUERY_INFO + CLOSE  security descriptor
```

Lookups share one connection: up to 16 directory queries (expansions and counts) and, separately, up to 32 security-descriptor queries in flight. Lookups start in top-level order — everything under an earlier top-level entry before anything under a later one — and directory queries go on the wire before security descriptors. An entry is ready once its lookups finish — for an expanded directory, also all of its children, whose count becomes `childCount` without another query, so each directory is opened once. Ready top-level entries are appended in server order to the task's `EntryStream`, which also stores the final result. Each subscriber is woken with a payload-free signal at most once until it drains; `subscribeEntries` then pulls every entry since the last drain as one coalesced batch, so a busy JS thread does not queue one callback per native flush.

The same engine backs every other directory walk through `listing::listOnCtx`, a non-streaming call on an already-held slot; libsmb2's `opendir` is no longer used:

| Caller | Listing |
| ------ | ------- |
| `deleteItem` | whole tree once (`maxDepth: -1`, no counts), then pipelined deletes (see above) |
| `copyItem`, `duplicateItem` | whole source tree once; total size and copy plan come from it |
| `duplicateItem` naming | parent filtered by the server to `"<base> copy*"`, no counts |
| `getPathInfo` | none for files: `smb2_stat` and the SD query run together; folders read their own entries once for `childCount` |

## Diagnostics

```typescript
const info = smb.getPoolInfo();
const id = smb.subscribePoolInfo((snap) => console.log(snap));
smb.unsubscribePoolInfo(id);
```

`PoolSlotInfo`: `index`, `interactiveOnly`, `state` (`Idle` | `Assigned` | `Activating`), `isConnected`, `shareName`, `taskId`, `kind`.

- Same `index` turning over quickly → tasks queuing on one slot.
- Slots 1–3 all `Assigned`, metadata still pending → pool full.
- Interactive `kind` on slot 1–3 → slot 0 was busy.

`example/components/PoolDebugPanel.js`