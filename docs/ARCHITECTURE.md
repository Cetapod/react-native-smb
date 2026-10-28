# Connection pool

`HybridSMB` runs up to four SMB operations in parallel through a `SmbConnectionPool(4)`. Each slot is one `SmbConnectionManager` / `smb2_context` — its own TCP socket and session:

```
TCP → NEGOTIATE → SESSION SETUP → TREE CONNECT
```

Tasks call `requestContext(mode)`, hold a slot for the operator's lifetime, then release via `PoolContextHandle`. One operator per slot (mutex on the context). Work on different slots runs concurrently.

From `SmbConnectionPool.hpp`:

```
slot[0]       — Interactive-only
slot[1..N-1]  — General (any AcquireMode; Interactive may overflow here)
```

| Slot | Accepts | Typical work |
| ---- | ------- | ------------ |
| `slot[0]` | `Interactive` only | `listDirectory`, `getPathInfo`, rename, move |
| `slot[1..3]` | Any | Download, upload, copy, delete; grows on demand |

```cpp
bool PoolSlot::accepts(AcquireMode mode) const {
    if (index_ == 0) return mode == AcquireMode::Interactive;
    return true;
}
```

Metadata never uses slot 0. Interactive work can overflow to slots 1–3 when slot 0 is busy.

### Acquire modes

| Mode | Operators |
| ---- | --------- |
| `Interactive` | `getPathInfo`, `listDirectory` (`maxDepth: 0`), `moveItem`, `renameItem`, `createDirectory`, `getSecurityDescriptor`, `listShares` |
| `Metadata` | `downloadFile`, `uploadFile`, `copyItem`, `duplicateItem`, `deleteItem`, `listDirectory` (`maxDepth != 0`) |

FIFO queue per mode.

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

## Listing pipeline

`listDirectory` runs `DirectoryLister` on one slot — Interactive for `maxDepth: 0`, Metadata for nested listings — instead of libsmb2's `smb2_opendir` (which reads the whole directory before returning):

```
[CREATE + QUERY_DIRECTORY] → QUERY_DIRECTORY … → CLOSE       decode each reply as it arrives
   └ directory within maxDepth: same chain, decoded          expand into children
   └ directory at the limit:    same chain, counted          childCount
   └ per entry (opt-in):        CREATE + QUERY_INFO + CLOSE  security descriptor
```

Lookups share one connection: up to 16 directory queries (expansions and counts) and, separately, up to 32 security-descriptor queries in flight. Lookups start in top-level order — everything under an earlier top-level entry before anything under a later one — and directory queries go on the wire before security descriptors. An entry is ready once its lookups finish — for an expanded directory, also all of its children, whose count becomes `childCount` without another query, so each directory is opened once. Ready top-level entries are emitted in server order through the task's `EntryStream` (`subscribeEntries`), which also stores the final result.

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