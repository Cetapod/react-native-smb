import React, { useState, useEffect } from 'react';
import { Modal, StyleSheet, Text, TextInput, TouchableOpacity, View, FlatList, ActivityIndicator, Platform } from 'react-native';

import { isAccessDeniedError } from '../utils/errorHandler';

const FolderPickerModal = ({ visible, onClose, onSelect, currentPath, SMB, initialShare, showAlert, currentUsername }) => {
  const [currentPickerPath, setCurrentPickerPath] = useState(currentPath || '/');
  const [items, setItems] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [pathHistory, setPathHistory] = useState([currentPath || '/']);
  const [showCreateFolderModal, setShowCreateFolderModal] = useState(false);
  const [newFolderName, setNewFolderName] = useState('');

  useEffect(() => {
    if (visible) {
      const startPath = currentPath || '/';
      setCurrentPickerPath(startPath);
      setPathHistory([startPath]);
      loadItems(startPath);
    }
  }, [visible, currentPath]);

  const loadItems = async (path) => {
    try {
      setIsLoading(true);
      const files = await SMB.listDirectory(path, false, -1).result();
      const sortedFiles = files.sort((a, b) => {
        if (a.isDirectory && !b.isDirectory) return -1;
        if (!a.isDirectory && b.isDirectory) return 1;
        return a.name.localeCompare(b.name);
      });
      setItems(sortedFiles);
    } catch (error) {
      if (path === '/' && currentUsername && isAccessDeniedError(error)) {
        try {
          const userPath = `/${currentUsername}`;
          const files = await SMB.listDirectory(userPath, false, -1).result();
          const sortedFiles = files.sort((a, b) => {
            if (a.isDirectory && !b.isDirectory) return -1;
            if (!a.isDirectory && b.isDirectory) return 1;
            return a.name.localeCompare(b.name);
          });
          setItems(sortedFiles);
          setCurrentPickerPath(userPath);
          showAlert('Info', `Root directory not accessible. Showing your home directory instead.`);
          return;
        } catch {
          showAlert('Error', `Failed to load items: ${error.message}`);
          setItems([]);
          return;
        }
      } else {
        showAlert('Error', `Failed to load items: ${error.message}`);
        setItems([]);
      }
    } finally {
      setIsLoading(false);
    }
  };

  const handleFolderPress = async (folder) => {
    const newPath = currentPickerPath === '/' ? `/${folder.name}` : `${currentPickerPath}/${folder.name}`;
    setCurrentPickerPath(newPath);
    setPathHistory([...pathHistory, newPath]);
    await loadItems(newPath);
  };

  const handleGoBack = async () => {
    if (currentPickerPath === '/') {
      return;
    }

    // Get parent folder path
    const pathParts = currentPickerPath.split('/').filter((p) => p);
    pathParts.pop(); // Remove last folder
    const parentPath = pathParts.length === 0 ? '/' : '/' + pathParts.join('/');

    setCurrentPickerPath(parentPath);
    await loadItems(parentPath);
  };

  const canGoBack = () => {
    return currentPickerPath !== '/';
  };

  const handleSelectCurrent = () => {
    if (currentPickerPath === currentPath) {
      showAlert('Invalid Selection', 'Cannot move to the same location');
      return;
    }
    onSelect(currentPickerPath);
    onClose();
  };

  const handleCreateFolder = async () => {
    if (!newFolderName.trim()) {
      showAlert('Error', 'Please enter a folder name');
      return;
    }

    try {
      setIsLoading(true);
      const folderPath = currentPickerPath === '/' ? `/${newFolderName.trim()}` : `${currentPickerPath}/${newFolderName.trim()}`;
      await SMB.createDirectory(folderPath).result();
      setNewFolderName('');
      setShowCreateFolderModal(false);
      await loadItems(currentPickerPath);
      showAlert('Success', 'Folder created successfully');
    } catch (error) {
      showAlert('Error', `Failed to create folder: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  const getCurrentFolderName = () => {
    if (currentPickerPath === '/') {
      return initialShare || 'Root';
    }
    const parts = currentPickerPath.split('/').filter((p) => p);
    return parts[parts.length - 1] || initialShare || 'Root';
  };

  const renderItem = ({ item }) => {
    const isFolder = item.isDirectory;
    return (
      <TouchableOpacity
        style={[styles.item, !isFolder && styles.itemDisabled]}
        onPress={() => isFolder && handleFolderPress(item)}
        disabled={!isFolder}>
        <View style={[styles.itemIcon, !isFolder && styles.itemIconDisabled]}>
          <Text style={styles.itemIconText}>{isFolder ? '📁' : '📄'}</Text>
        </View>
        <View style={styles.itemInfo}>
          <Text
            style={[styles.itemName, !isFolder && styles.itemNameDisabled]}
            numberOfLines={1}>
            {item.name}
          </Text>
          {!isFolder && <Text style={styles.itemLabel}>File</Text>}
        </View>
        {isFolder && <Text style={styles.chevron}>›</Text>}
      </TouchableOpacity>
    );
  };

  return (
    <Modal
      visible={visible}
      animationType="slide"
      onRequestClose={onClose}>
      <View style={styles.fullscreenContainer}>
        <View style={styles.header}>
          <View style={styles.safeAreaSpacer} />
          <View style={styles.headerContent}>
            <TouchableOpacity
              style={styles.closeButton}
              onPress={onClose}>
              <Text style={styles.closeButtonText}>✕</Text>
            </TouchableOpacity>
            <Text style={styles.title}>Select Destination</Text>
            <TouchableOpacity
              style={styles.newFolderButton}
              onPress={() => setShowCreateFolderModal(true)}>
              <Text style={styles.newFolderButtonText}>+</Text>
            </TouchableOpacity>
          </View>
        </View>

        <View style={styles.pathContainer}>
          <View style={styles.pathRow}>
            <TouchableOpacity
              style={[styles.backButton, !canGoBack() && styles.backButtonDisabled]}
              onPress={handleGoBack}
              disabled={!canGoBack()}>
              <Text style={[styles.backButtonText, !canGoBack() && styles.backButtonTextDisabled]}>←</Text>
            </TouchableOpacity>
            <Text
              style={styles.currentPathText}
              numberOfLines={1}>
              {getCurrentFolderName()}
            </Text>
          </View>
        </View>

        {isLoading ? (
          <View style={styles.loadingContainer}>
            <ActivityIndicator
              size="large"
              color="#3B82F6"
            />
            <Text style={styles.loadingText}>Loading...</Text>
          </View>
        ) : items.length === 0 ? (
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>Empty Folder</Text>
            <Text style={styles.emptySubtext}>This folder is empty</Text>
          </View>
        ) : (
          <FlatList
            data={items}
            renderItem={renderItem}
            keyExtractor={(item, index) => `item-${index}-${item.name}`}
            style={styles.itemsList}
            contentContainerStyle={styles.itemsListContent}
          />
        )}

        <View style={styles.actionsContainer}>
          <TouchableOpacity
            style={styles.selectButton}
            onPress={handleSelectCurrent}>
            <Text style={styles.selectButtonText}>Move Here</Text>
          </TouchableOpacity>
        </View>

        <Modal
          visible={showCreateFolderModal}
          transparent={true}
          animationType="slide"
          onRequestClose={() => setShowCreateFolderModal(false)}>
          <View style={styles.createModalOverlay}>
            <View style={styles.createModalContent}>
              <Text style={styles.createModalTitle}>New Folder</Text>
              <TextInput
                style={styles.createModalInput}
                placeholder="Enter folder name"
                value={newFolderName}
                onChangeText={setNewFolderName}
                autoFocus={true}
              />
              <View style={styles.createModalButtons}>
                <TouchableOpacity
                  style={[styles.createModalButton, styles.createModalButtonCancel]}
                  onPress={() => {
                    setShowCreateFolderModal(false);
                    setNewFolderName('');
                  }}>
                  <Text style={styles.createModalButtonText}>Cancel</Text>
                </TouchableOpacity>
                <TouchableOpacity
                  style={[styles.createModalButton, styles.createModalButtonConfirm]}
                  onPress={handleCreateFolder}>
                  <Text style={[styles.createModalButtonText, styles.createModalButtonTextConfirm]}>Create</Text>
                </TouchableOpacity>
              </View>
            </View>
          </View>
        </Modal>
      </View>
    </Modal>
  );
};

const styles = StyleSheet.create({
  fullscreenContainer: {
    flex: 1,
    backgroundColor: '#F0F8FF',
  },
  safeAreaSpacer: {
    height: Platform.OS === 'ios' ? 50 : 16,
  },
  header: {
    backgroundColor: '#FFFFFF',
    borderBottomWidth: 1,
    borderBottomColor: '#E5E7EB',
  },
  headerContent: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 16,
    paddingVertical: 12,
  },
  title: {
    flex: 1,
    fontSize: 18,
    fontWeight: '600',
    color: '#111827',
    textAlign: 'center',
  },
  closeButton: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: '#F3F4F6',
    justifyContent: 'center',
    alignItems: 'center',
  },
  closeButtonText: {
    fontSize: 20,
    color: '#6B7280',
    fontWeight: '600',
  },
  newFolderButton: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: '#3B82F6',
    justifyContent: 'center',
    alignItems: 'center',
  },
  newFolderButtonText: {
    fontSize: 24,
    color: '#FFFFFF',
    fontWeight: '300',
  },
  pathContainer: {
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#FFFFFF',
    borderBottomWidth: 1,
    borderBottomColor: '#E5E7EB',
  },
  pathRow: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  backButton: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: '#FFFFFF',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
    borderWidth: 1,
    borderColor: '#E5E7EB',
  },
  backButtonDisabled: {
    backgroundColor: '#F3F4F6',
    borderColor: '#E5E7EB',
  },
  backButtonText: {
    fontSize: 18,
    fontWeight: '600',
    color: '#3B82F6',
  },
  backButtonTextDisabled: {
    color: '#D1D5DB',
  },
  currentPathText: {
    flex: 1,
    fontSize: 16,
    fontWeight: '600',
    color: '#111827',
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    padding: 40,
  },
  loadingText: {
    marginTop: 12,
    fontSize: 14,
    color: '#6B7280',
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    padding: 40,
  },
  emptyText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#6B7280',
    marginBottom: 8,
  },
  emptySubtext: {
    fontSize: 14,
    color: '#9CA3AF',
    textAlign: 'center',
  },
  itemsList: {
    flex: 1,
  },
  itemsListContent: {
    padding: 8,
  },
  item: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#FFFFFF',
    borderRadius: 8,
    marginVertical: 2,
    borderWidth: 1,
    borderColor: '#E5E7EB',
  },
  itemDisabled: {
    opacity: 0.5,
    backgroundColor: '#F9FAFB',
  },
  itemIcon: {
    width: 40,
    height: 40,
    backgroundColor: '#EFF6FF',
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  itemIconDisabled: {
    backgroundColor: '#F3F4F6',
  },
  itemIconText: {
    fontSize: 20,
  },
  itemInfo: {
    flex: 1,
  },
  itemName: {
    fontSize: 16,
    fontWeight: '500',
    color: '#111827',
  },
  itemNameDisabled: {
    color: '#6B7280',
  },
  itemLabel: {
    fontSize: 12,
    color: '#9CA3AF',
    marginTop: 2,
  },
  chevron: {
    fontSize: 20,
    color: '#9CA3AF',
    fontWeight: '300',
  },
  actionsContainer: {
    padding: 16,
    paddingBottom: Platform.OS === 'ios' ? 34 : 16,
    borderTopWidth: 1,
    borderTopColor: '#E5E7EB',
    backgroundColor: '#FFFFFF',
  },
  selectButton: {
    paddingVertical: 14,
    paddingHorizontal: 24,
    backgroundColor: '#3B82F6',
    borderRadius: 10,
    alignItems: 'center',
  },
  selectButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#FFFFFF',
  },
  createModalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(128, 128, 128, 0.4)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  createModalContent: {
    width: '80%',
    backgroundColor: '#FFFFFF',
    borderRadius: 12,
    padding: 20,
    borderWidth: 1,
    borderColor: '#B8D4F1',
    shadowColor: '#4A90E2',
    shadowOffset: { width: 0, height: 6 },
    shadowOpacity: 0.2,
    shadowRadius: 12,
    elevation: 8,
  },
  createModalTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#1E3A5F',
    marginBottom: 16,
    textAlign: 'center',
  },
  createModalInput: {
    height: 44,
    backgroundColor: '#F0F8FF',
    borderRadius: 8,
    paddingHorizontal: 12,
    fontSize: 16,
    marginBottom: 20,
    borderWidth: 1,
    borderColor: '#B8D4F1',
    color: '#1E3A5F',
  },
  createModalButtons: {
    flexDirection: 'row',
    gap: 12,
  },
  createModalButton: {
    flex: 1,
    height: 44,
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
  },
  createModalButtonCancel: {
    backgroundColor: '#F0F8FF',
    borderWidth: 1,
    borderColor: '#B8D4F1',
  },
  createModalButtonConfirm: {
    backgroundColor: '#4A90E2',
  },
  createModalButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#1E3A5F',
  },
  createModalButtonTextConfirm: {
    color: '#FFFFFF',
  },
});

export default FolderPickerModal;
