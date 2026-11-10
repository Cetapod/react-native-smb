/**
 * Structs shared by Nitrogen HybridObject specs.
 * Domain C++ keeps its own SmbModels with int64_t; bridge maps between them.
 * SmbFileInfo is intentionally absent — Nitrogen 0.37.2 cannot generate
 * recursive structs; it stays hand-converted via JSIConverter.
 */

export interface SmbCredentials {
  username: string
  password: string
}

export interface SmbShare {
  name: string
  comment: string
}

export interface SmbConnectionInfo {
  url: string
  server: string
  share: string
  isConnected: boolean
}

export interface SmbAce {
  aceType: string
  aceFlags: string[]
  mask: string[]
  sid: string
}

export interface SmbAcl {
  revision: number
  aceCount: number
  aces: SmbAce[]
}

export interface SmbSecurityDescriptor {
  revision: number
  control: string[]
  ownerSid: string
  groupSid: string
  dacl: SmbAcl
}
