import type { HybridObject } from 'react-native-nitro-modules'

/**
 * Nitro HybridObject for one native SMB operation.
 *
 * `getResultValue` is registered as a raw JSI method in HybridSmbTask
 * (result type varies per operator). Listing entry streaming lives on
 * NativeSmbListTask.
 */
export interface NativeSmbTask extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  cancel(): void
  get(): Record<string, string>
  subscribe(listener: (snapshot: Record<string, string>) => void): string
  unsubscribe(subscriptionId: string): void
}
