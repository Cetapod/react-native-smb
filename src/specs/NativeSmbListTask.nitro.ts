import type { NativeSmbTask } from './NativeSmbTask.nitro'

/**
 * Listing HybridObject. Extends NativeSmbTask with entry-stream signals.
 * `drainEntries` stays a raw JSI method (recursive SmbFileInfo).
 */
export interface NativeSmbListTask extends NativeSmbTask {
  subscribeEntries(listener: () => void): string
  unsubscribeEntries(subscriptionId: string): void
}
