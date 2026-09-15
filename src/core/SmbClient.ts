import { NitroModules } from 'react-native-nitro-modules';
import { shapePoolInfo } from '../bridge/decode';
import type { ReactNativeSmb } from '../ReactNativeSmb';
import type {
  NativeSmbTask,
  PoolInfo,
  SmbConnectionInfo,
  SmbCredentials,
  SmbFileInfo,
  SmbSecurityDescriptor,
  SmbShare,
} from '../types';
import { generateTaskId } from './ids';
import { SmbTask } from './SmbTask';
import { TransferStore } from './TransferStore';

export class SmbClient {
  private readonly native: ReactNativeSmb;
  private transfers_?: TransferStore;

  constructor(native?: ReactNativeSmb) {
    this.native =
      native ?? NitroModules.createHybridObject<ReactNativeSmb>('ReactNativeSmb');
  }

  private op<T>(call: (taskId: string) => NativeSmbTask): SmbTask<T> {
    const id = generateTaskId();
    return new SmbTask<T>(id, call(id));
  }

  transferStore(): TransferStore {
    if (!this.transfers_) {
      this.transfers_ = new TransferStore(this.native);
    }
    return this.transfers_;
  }

  /**
   * Cancel tasks, disconnect the pool, wait for workers, and stop transfer tracking.
   * Not reusable — create a new client with `SMB()` afterwards.
   */
  async destroy(): Promise<void> {
    await this.native.destroy();
    this.transfers_?.stop();
    this.transfers_ = undefined;
  }

  isConnected(): boolean {
    return this.native.isConnected();
  }

  isInitialized(): boolean {
    return this.native.isInitialized();
  }

  initialize(url: string, creds: SmbCredentials): SmbTask<void> {
    return this.op((id) => this.native.initialize(id, url, creds));
  }

  connect(url: string, creds: SmbCredentials): SmbTask<SmbConnectionInfo> {
    return this.op((id) => this.native.connect(id, url, creds));
  }

  connectShare(share: string): SmbTask<SmbConnectionInfo> {
    return this.op((id) => this.native.connectShare(id, share));
  }

  disconnect(): SmbTask<void> {
    return this.op((id) => this.native.disconnect(id));
  }

  listShares(): SmbTask<SmbShare[]> {
    return this.op((id) => this.native.listShares(id));
  }

  listDirectory(path: string, recursive = false, maxDepth = -1, includeSecurityDescriptor = false): SmbTask<SmbFileInfo[]> {
    return this.op((id) => this.native.listDirectory(id, path, recursive, maxDepth, includeSecurityDescriptor));
  }

  getPathInfo(path: string): SmbTask<SmbFileInfo> {
    return this.op((id) => this.native.getPathInfo(id, path));
  }

  getSecurityDescriptor(path: string): SmbTask<SmbSecurityDescriptor> {
    return this.op((id) => this.native.getSecurityDescriptor(id, path));
  }

  downloadFile(remotePath: string, localPath: string): SmbTask<void> {
    return this.op((id) => this.native.downloadFile(id, remotePath, localPath));
  }

  uploadFile(localPath: string, remotePath: string): SmbTask<void> {
    return this.op((id) => this.native.uploadFile(id, localPath, remotePath));
  }

  createDirectory(path: string): SmbTask<void> {
    return this.op((id) => this.native.createDirectory(id, path));
  }

  deleteItem(path: string): SmbTask<void> {
    return this.op((id) => this.native.deleteItem(id, path));
  }

  moveItem(from: string, to: string): SmbTask<void> {
    return this.op((id) => this.native.moveItem(id, from, to));
  }

  renameItem(path: string, newName: string): SmbTask<void> {
    return this.op((id) => this.native.renameItem(id, path, newName));
  }

  copyItem(from: string, to: string, recursive = false): SmbTask<void> {
    return this.op((id) => this.native.copyItem(id, from, to, recursive));
  }

  duplicateItem(path: string): SmbTask<string> {
    return this.op((id) => this.native.duplicateItem(id, path));
  }

  cancelTask(taskId: string): void {
    this.native.cancelTask(taskId);
  }

  cancelTransferTasks(): void {
    this.native.cancelTransferTasks();
  }

  getPoolInfo(): PoolInfo {
    return shapePoolInfo(this.native.getPoolInfo());
  }

  subscribePoolInfo(cb: (info: PoolInfo) => void): string {
    const id = this.native.subscribePoolInfo((raw) => {
      cb(shapePoolInfo(raw));
    });
    cb(this.getPoolInfo());
    return id;
  }

  unsubscribePoolInfo(id: string | null): void {
    if (id) this.native.unsubscribePoolInfo(id);
  }

  resetPool(): void {
    this.native.resetPool();
  }
}
