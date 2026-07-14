import { SmbClient } from './SmbClient';

export function SMB(): SmbClient {
  return new SmbClient();
}

export type Smb = SmbClient;