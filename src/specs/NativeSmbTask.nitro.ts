import type { HybridObject } from 'react-native-nitro-modules'

/**
 * Nitro HybridObject for one native SMB operation.
 *
 * `getResultValue` and `drainEntries` are registered as raw JSI methods in
 * HybridSmbTask (result type varies; SmbFileInfo is recursive and cannot be
 * Nitrogen-generated).
 */
export interface NativeSmbTask extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  cancel(): void
  get(): Record<string, string>
  subscribe(listener: (snapshot: Record<string, string>) => void): string
  unsubscribe(subscriptionId: string): void
  subscribeEntries(listener: () => void): string
  unsubscribeEntries(subscriptionId: string): void
}
