import type { HybridObject } from 'react-native-nitro-modules';
import type { SmbCredentials, SmbShareList, SmbConnectionInfo, SmbFileInfo, SmbSecurityDescriptor, NativeSmbTask } from './types';

/**
 * A Nitro module that provides SMB client functionality using libsmb2
 *
 * This interface provides low-level access to SMB operations with proper
 * TypeScript typing for React Native applications.
 */
export interface ReactNativeSmb extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  // State Checks
  // ----------------------------------------------

  /**
   * Checks connection status
   * @returns True if currently connected to an SMB server
   */
  isConnected(): boolean;

  /**
   * Checks if the SMB client is initialized
   * @returns True if initialized, false otherwise
   */
  isInitialized(): boolean;

  // Connection Management
  // ----------------------------------------------

  /**
   * Initializes connection to an SMB server (without connecting to a specific share)
   * @param url Server URL (e.g., "smb://server-ip")
   * @param credentials Authentication credentials
   * @returns SmbTask that resolves when initialized
   */
  initialize(taskId: string, url: string, credentials: SmbCredentials): NativeSmbTask;

  /**
   * Establishes a connection to an SMB server
   * @param url The SMB server URL (e.g., "smb://server-ip/share")
   * @param credentials Authentication credentials
   * @returns SmbTask with connection information
   * @throws {Error} When connection fails
   *
   * @example
   * await smb.connect('smb://192.168.1.100', {
   *   username: 'admin',
   *   password: 'secret'
   * });
   */
  connect(taskId: string, url: string, credentials: SmbCredentials): NativeSmbTask;

  /**
   * Connects to a specific share
   * @param share Name of the share to connect to
   * @returns SmbTask that resolves when connected
   * @throws {Error} If not initialized or share doesn't exist
   */
  connectShare(taskId: string, share: string): NativeSmbTask;

  /**
   * Disconnects from the current SMB server
   * @returns SmbTask that resolves when disconnected
   */
  disconnect(taskId: string): NativeSmbTask;


  /**
   * Lists available shares on the server
   * @returns SmbTask resolving to array of share information
   * @throws {Error} If not initialized
   *
   * @example
   * const shares = await smb.listShares();
   * shares.forEach(share => console.log(share.name));
   */
  listShares(taskId: string): NativeSmbTask;


  // Listing & Info
  // ----------------------------------------------

  /**
   * Lists contents of a directory
   * @param path Directory path (use "/" for root)
   * @param recursive Whether to list recursively (default: false)
   * @param maxDepth Maximum recursion depth (-1 = unlimited, 0 = current only)
   * @param taskId Unique ID for the task
   * @param onStatusChange Callback for status updates
   * @param onProgress Callback for progress updates
   * @returns SmbTask resolving to array of file/directory info
   */
  listDirectory(taskId: string, path: string, recursive: boolean, maxDepth: number): NativeSmbTask;


  /**
   * Gets detailed information about a path (file or directory).
   * Generic version that handles both file/directory types.
   * @param path Path to the item
   * @param taskId Unique ID for the task
   * @param onStatusChange Callback for status updates
   * @param onProgress Callback for progress updates
   * @returns SmbTask resolving to file information
   */
  getPathInfo(taskId: string, path: string): NativeSmbTask;


  /**
   * Retrieves security descriptor for a file/directory
   * @param path Path to the item
   * @param taskId Unique ID for the task
   * @returns SmbTask resolving to security descriptor
   *
   * @example
   * const security = await smb.getSecurityDescriptor('/important.txt');
   * console.log('Owner:', security.ownerSid);
   */
  getSecurityDescriptor(taskId: string, path: string): NativeSmbTask;


  // File Transfers
  // ----------------------------------------------

  downloadFile(taskId: string, remotePath: string, localPath: string): NativeSmbTask;


  /**
   * Uploads a file from local storage to the SMB server
   * @param localPath Local path to the file
   * @param remotePath Destination path on the server
   * @param progressHandler Optional callback to track upload progress
   * @returns SmbTask that resolves when upload completes
   */
  uploadFile(taskId: string, localPath: string, remotePath: string): NativeSmbTask;


  // Mutating File Operations
  // ----------------------------------------------

  /**
   * Creates a new directory
   * @param path Path of the directory to create
   * @returns SmbTask that resolves when directory is created
   */
  createDirectory(taskId: string, path: string): NativeSmbTask;


  /**
   * Deletes a file or directory (automatically handles recursive deletion)
   * @param path Path to the item to delete
   * @returns SmbTask that resolves when deletion completes
   */
  deleteItem(taskId: string, path: string): NativeSmbTask;


  /**
   * Moves or renames a file/directory
   * @param fromPath Current path
   * @param toPath New path
   * @returns SmbTask that resolves when move completes
   */
  moveItem(taskId: string, fromPath: string, toPath: string): NativeSmbTask;


  /**
   * Renames a file/directory (in the same directory)
   * @param currentPath Current path
   * @param newName New name (without path)
   * @returns SmbTask that resolves when rename completes
   */
  renameItem(taskId: string, currentPath: string, newName: string): NativeSmbTask;


  /**
   * Copies a file or directory on the SMB server
   * @param fromPath Source path
   * @param toPath Destination path
   * @param recursive Whether to copy recursively
   * @param taskId Unique ID for the task
   * @param onStatusChange Callback for status updates
   * @param onProgress Callback for progress updates
   */
  copyItem(taskId: string, fromPath: string, toPath: string, recursive: boolean): NativeSmbTask;


  /**
   * Duplicates an item giving it a unique name
   * @param path Path to the item
   * @returns SmbTask resolving to the new path
   */
  duplicateItem(taskId: string, path: string): NativeSmbTask;

  // Task control

  /**
   * Subscribe to task events (created, status, progress, settled, removed)
   * @param listener Callback that receives task events
   * @returns Subscription ID for unsubscribing
   */
  subscribeTaskEvents(listener: (event: Record<string, string>) => void): string;

  /**
   * Unsubscribe from task events
   * @param subscriptionId The subscription ID returned from subscribeTaskEvents
   */
  unsubscribeTaskEvents(subscriptionId: string): void;

  /**
   * Get a specific task snapshot by ID
   * @param taskId The task ID to retrieve
   * @returns Task snapshot as string-keyed map, or {exists: '0'} if not found
   */
  getTask(taskId: string): Record<string, string>;

  /**
   * Get all currently active (pending/running) tasks
   * @returns Array of task snapshots
   */
  getActiveTasks(): Array<Record<string, string>>;

  /**
   * Get task history (settled tasks)
   * @param limit Maximum number of tasks to return
   * @param offset Number of tasks to skip
   * @returns Array of task snapshots
   */
  getTaskHistory(limit: number, offset: number): Array<Record<string, string>>;

  /**
   * Cancels a specific task by ID
   */
  cancelTask(taskId: string): void;

  /**
   * Clear task history
   * @param beforeTs Clear tasks that ended before this timestamp (0 = clear all)
   */
  clearTaskHistory(beforeTs: bigint): void;

  // Debug / instrumentation

  subscribePoolInfo(listener: (snapshot: Array<{ [key: string]: string }>) => void): string;

  unsubscribePoolInfo(subscriptionId: string): void;

  getPoolInfo(): Array<{ [key: string]: string }>;

  /**
   * Force-disconnects every connection-pool slot and forgets cached config.
   * Use for recovery when slots become stuck (`isConnected:false` but
   * `inUse:true`). Subsequent operations must re-connect from scratch.
   */
  resetPool(): void;
}
