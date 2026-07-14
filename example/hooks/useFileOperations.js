import * as DocumentPicker from 'expo-document-picker';
import { Paths } from 'expo-file-system/next';
import * as Sharing from 'expo-sharing';

import { SmbTask } from '@cetapod/react-native-smb';

import { buildFilePath } from '../utils/connectionUtils';
import { generateUniqueName } from '../utils/uniqueName';

export const useFileOperations = (SMB, currentPath, loadFiles, setIsLoading, showAlert) => {
  const runWithSpinner = async (operation, reloadAfter = false) => {
    try {
      setIsLoading(true);
      const result = await operation();
      if (reloadAfter) await loadFiles(currentPath);
      return result;
    } catch (error) {
      showAlert('Error', error.message || 'An unknown error occurred');
      throw error;
    } finally {
      setIsLoading(false);
    }
  };

  const checkFileExists = async (fileName) => {
    try {
      const files = await SMB.listDirectory(currentPath, false, -1).result();
      return files.some((file) => file.name === fileName);
    } catch (error) {
      console.warn('checkFileExists failed:', error);
      return false;
    }
  };

  const promptOnFileConflict = (fileName) =>
    new Promise((resolve) => {
      showAlert('File Already Exists', `"${fileName}" already exists. What would you like to do?`, [
        { text: 'Cancel', onPress: () => resolve('cancel') },
        { text: 'Keep Both', onPress: () => resolve('keep-both') },
        { text: 'Replace', onPress: () => resolve('replace') },
      ]);
    });

  const downloadFile = async (file, opts = {}) => {
    const { autoShare = true, onTask } = opts;
    const remotePath = buildFilePath(currentPath, file.name);
    const localFilePath = buildFilePath(Paths.cache.uri, file.name);

    try {
      const task = SMB.downloadFile(remotePath, localFilePath);
      if (typeof onTask === 'function') onTask(task);

      await task.result();

      if (typeof onTask === 'function') onTask(null);

      if (autoShare) {
        try {
          if (await Sharing.isAvailableAsync()) {
            await Sharing.shareAsync(localFilePath);
          }
        } catch (shareErr) {
          console.warn('shareAsync failed:', shareErr);
        }
      }
      return { ok: true };
    } catch (error) {
      if (typeof onTask === 'function') onTask(null);
      showAlert('Download Failed', error.message || 'Download failed');
      return { ok: false, error };
    }
  };

  const uploadFile = async () => {
    const result = await DocumentPicker.getDocumentAsync({ multiple: true });
    if (result.canceled) return;
    const assets = result.assets && result.assets.length ? result.assets : result.uri ? [result] : [];
    if (!assets.length) {
      showAlert('Error', 'No files selected');
      return;
    }

    let uploadedCount = 0;
    for (const asset of assets) {
      if (!asset?.uri || !asset?.name) continue;

      let finalName = asset.name;
      if (await checkFileExists(finalName)) {
        const action = await promptOnFileConflict(finalName);
        if (action === 'cancel') continue;
        if (action === 'keep-both') {
          const existing = await SMB.listDirectory(currentPath, false, -1).result();
          finalName = generateUniqueName(
            asset.name,
            existing.map((f) => f.name),
          );
        }
      }

      const remotePath = buildFilePath(currentPath, finalName);
      try {
        await SMB.uploadFile(asset.uri, remotePath).result();
        uploadedCount++;
      } catch (error) {
        showAlert('Upload Failed', error.message || 'Upload failed');
      }
    }

    if (uploadedCount > 0) await loadFiles(currentPath);
  };

  const deleteFile = async (file) => {
    const filePath = buildFilePath(currentPath, file.name);
    try {
      await SMB.deleteItem(filePath).result();
      await loadFiles(currentPath);
    } catch (error) {
      showAlert('Delete Failed', error.message || 'Delete failed');
    }
  };

  const executeFileOperation = async (selectedFile, fileOperationType, inputValue) => {
    const fromPath = buildFilePath(currentPath, selectedFile.name);

    if (fileOperationType === 'rename') {
      await runWithSpinner(() => SMB.renameItem(fromPath, inputValue).result(), true);
      return;
    }

    if (fileOperationType === 'move') {
      try {
        await SMB.moveItem(fromPath, inputValue).result();
        await loadFiles(currentPath);
      } catch (error) {
        showAlert('Move Failed', error.message || 'Move failed');
      }
      return;
    }

    if (fileOperationType === 'copy') {
      const copyPath = buildFilePath(currentPath, inputValue);
      try {
        await SMB.copyItem(fromPath, copyPath, selectedFile.isDirectory).result();
        await loadFiles(currentPath);
      } catch (error) {
        showAlert('Copy Failed', error.message || 'Copy failed');
      }
      return;
    }

    throw new Error(`Unknown operation: ${fileOperationType}`);
  };

  const createFolder = async (folderName) => {
    const folderPath = buildFilePath(currentPath, folderName);
    await runWithSpinner(() => SMB.createDirectory(folderPath).result(), true);
  };

  const duplicateFile = async (file) => {
    const path = buildFilePath(currentPath, file.name);
    try {
      await SMB.duplicateItem(path).result();
      await loadFiles(currentPath);
    } catch (error) {
      showAlert('Duplicate Failed', error.message || 'Duplicate failed');
    }
  };

  const duplicateFiles = async (files) => {
    if (!files || files.length === 0) return;
    let existing = [];
    try {
      const list = await SMB.listDirectory(currentPath, false, -1).result();
      existing = (list || []).map((f) => f.name);
    } catch {}
    const reserved = new Set();
    const tasks = files.map((file) => {
      const targetName = generateUniqueName(file.name, existing, reserved);
      reserved.add(targetName);
      const fromPath = buildFilePath(currentPath, file.name);
      const toPath = buildFilePath(currentPath, targetName);
      return SMB.copyItem(fromPath, toPath, !!file.isDirectory);
    });

    void (async () => {
      const results = await SmbTask.settleAll(tasks);
      results.forEach((result, index) => {
        if (result.status === 'rejected') {
          const file = files[index];
          showAlert('Duplicate Failed', result.reason?.message || `Duplicate failed for ${file?.name}`);
        }
      });
      await loadFiles(currentPath);
    })();
  };

  const moveFiles = async (files, destinationDir) => {
    if (!files || files.length === 0) return;
    const tasks = files.map((file) => {
      const fromPath = buildFilePath(currentPath, file.name);
      const toPath = destinationDir === '/' ? `/${file.name}` : `${destinationDir}/${file.name}`;
      return SMB.moveItem(fromPath, toPath);
    });

    void (async () => {
      const results = await SmbTask.settleAll(tasks);
      results.forEach((result, index) => {
        if (result.status === 'rejected') {
          const file = files[index];
          showAlert('Move Failed', result.reason?.message || `Move failed for ${file?.name}`);
        }
      });
      await loadFiles(currentPath);
    })();
  };

  const deleteFiles = async (files) => {
    if (!files || files.length === 0) return;
    const tasks = files.map((file) => {
      const filePath = buildFilePath(currentPath, file.name);
      return SMB.deleteItem(filePath);
    });

    void (async () => {
      const results = await SmbTask.settleAll(tasks);
      results.forEach((result, index) => {
        if (result.status === 'rejected') {
          const file = files[index];
          showAlert('Delete Failed', result.reason?.message || `Delete failed for ${file?.name}`);
        }
      });
      await loadFiles(currentPath);
    })();
  };

  return {
    downloadFile,
    uploadFile,
    deleteFile,
    deleteFiles,
    executeFileOperation,
    createFolder,
    duplicateFile,
    duplicateFiles,
    moveFiles,
  };
};