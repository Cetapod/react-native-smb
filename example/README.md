# Example app

Expo dev-client. 

After library changes:

```bash
npm run ios
```

| Component | API |
| ----------- | --- |
| Login / Shares | `connect`, `listShares`, `connectShare` |
| Files | `listDirectory`, file ops |
| `TaskSnapshotBar` | `useSubscribe` |
| `TransferTrayPanel` | `useTransfers` |
| `PoolDebugPanel` | `getPoolInfo` |
| ACL modal | `getSecurityDescriptor` |

Test on Wi‑Fi — port 445 is often blocked on cellular.