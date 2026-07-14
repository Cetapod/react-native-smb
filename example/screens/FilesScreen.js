import { useCallback, useRef, useState } from 'react';
import { FlatList, RefreshControl, SectionList, StyleSheet, Text, TextInput, TouchableOpacity, View } from 'react-native';

import { useMultiSelect } from '../hooks/useMultiSelect';
import CreateFolderModal from './files/CreateFolderModal';
import FabMenuModal from './files/FabMenuModal';
import FileRow from './files/FileRow';
import FolderMenuModal from './files/FolderMenuModal';
import SortMenuModal from './files/SortMenuModal';
import { filesStyles as styles } from './files/styles';
import { filterFiles, sortFiles } from './files/grouping';

const FilesScreen = ({ navigation, files, currentPath, selectedShare, username, onLoadFiles, onFileAction, onFolderInfo, onCreateFolder, onUploadFile, onRefresh, onBack, onLogout, isLoading, showAlert, showPrompt, onBulkDownload, onBulkDuplicate, onBulkDelete, onBulkMove, sortBy, sortOrder, showHidden, onChangeSort, onToggleHidden }) => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [newFolderName, setNewFolderName] = useState('');
  const [showFolderMenuModal, setShowFolderMenuModal] = useState(false);
  const [showSortMenuModal, setShowSortMenuModal] = useState(false);
  const [showFabMenuModal, setShowFabMenuModal] = useState(false);
  const pendingFabActionRef = useRef(null);
  const [refreshing, setRefreshing] = useState(false);
  const [collapsedSections, setCollapsedSections] = useState(new Set());
  const [showSearchBar, setShowSearchBar] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const multi = useMultiSelect();

  // Stable key per file (currentPath + name). Used both for selection set
  // membership AND FlatList/SectionList keyExtractor so React can reuse rows
  // across re-renders instead of remounting them on every progress tick.
  const fileKey = useCallback((item) => `${currentPath}|${item.name}`, [currentPath]);
  const selectionCount = multi.selected.size;

  const visibleFiles = useCallback(() => filterFiles(files, searchQuery).filter((f) => showHidden || !(f?.name || '').startsWith('.')), [files, searchQuery, showHidden]);

  const handleSelectAll = () => {
    const keys = visibleFiles().map(fileKey);
    multi.toggleAll(keys);
  };

  const selectedFileObjects = () => {
    const vis = visibleFiles();
    return vis.filter((f) => multi.isSelected(fileKey(f)));
  };

  const runBulk = async (kind) => {
    const items = selectedFileObjects();
    if (items.length === 0) return;
    if (kind === 'delete') {
      showAlert('Delete', `Delete ${items.length} item${items.length === 1 ? '' : 's'}? This cannot be undone.`, [
        { text: 'Cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: () => {
            multi.exit();
            onBulkDelete && onBulkDelete(items);
          },
        },
      ]);
      return;
    }
    if (kind === 'move') {
      onBulkMove && onBulkMove(items, () => multi.exit());
      return;
    }
    multi.exit();
    if (kind === 'download') onBulkDownload && onBulkDownload(items);
    else if (kind === 'duplicate') onBulkDuplicate && onBulkDuplicate(items);
  };

  const toggleSectionCollapse = (sectionTitle) => {
    setCollapsedSections((prev) => {
      const next = new Set(prev);
      if (next.has(sectionTitle)) next.delete(sectionTitle);
      else next.add(sectionTitle);
      return next;
    });
  };

  const handleRefresh = async () => {
    setRefreshing(true);
    try {
      await onRefresh();
    } finally {
      setRefreshing(false);
    }
  };

  const handleCreateFolder = () => {
    if (!newFolderName.trim()) {
      showAlert('Error', 'Please enter a folder name');
      return;
    }
    onCreateFolder(newFolderName.trim());
    setNewFolderName('');
    setShowCreateModal(false);
  };

  const getCurrentFolderName = () => {
    if (currentPath === '/') return selectedShare;
    const parts = currentPath.split('/').filter((p) => p !== '');
    return parts[parts.length - 1] || selectedShare;
  };

  const handleSortSelection = (newSortBy) => {
    let nextOrder = sortOrder;
    if (sortBy === newSortBy) {
      nextOrder = sortOrder === 'asc' ? 'desc' : 'asc';
    } else {
      nextOrder = newSortBy === 'date' ? 'desc' : 'asc';
    }
    onChangeSort && onChangeSort(newSortBy, nextOrder);
    setShowSortMenuModal(false);
  };

  const handleOpenFolder = (item) => {
    const newPath = currentPath === '/' ? `${item.name}` : `${currentPath}/${item.name}`;
    navigation.push('FilesView');
    onLoadFiles(newPath);
  };

  const renderFileItem = ({ item }) => (
    <FileRow
      item={item}
      onOpen={handleOpenFolder}
      onAction={onFileAction}
      selectionMode={multi.selectionMode}
      isSelected={multi.isSelected(fileKey(item))}
      onToggleSelect={(it) => multi.toggle(fileKey(it))}
    />
  );

  const renderSectionHeader = ({ section }) => {
    const isCollapsed = collapsedSections.has(section.title);
    return (
      <TouchableOpacity
        style={styles.sectionHeader}
        onPress={() => toggleSectionCollapse(section.title)}
        activeOpacity={0.7}>
        <View style={styles.sectionHeaderContent}>
          <Text style={styles.sectionTitle}>{section.title}</Text>
          <Text style={styles.sectionCount}>({section.count})</Text>
        </View>
        <Text style={[styles.sectionCollapseIcon, isCollapsed && styles.sectionCollapseIconCollapsed]}>{'\u25BC'}</Text>
      </TouchableOpacity>
    );
  };

  const renderListBody = () => {
    if (files.length === 0) {
      return (
        <View style={styles.emptyContainer}>
          <Text style={styles.emptyTitle}>{isLoading ? 'Loading...' : 'Folder is Empty'}</Text>
          <Text style={styles.emptySubtitle}>{isLoading ? 'Please wait' : 'No files or folders in this location'}</Text>
        </View>
      );
    }

    const filteredFiles = filterFiles(files, searchQuery).filter((f) => showHidden || !(f?.name || '').startsWith('.'));
    const sorted = sortFiles(filteredFiles, sortBy, sortOrder);

    const refreshControl = (
      <RefreshControl
        refreshing={refreshing || isLoading}
        onRefresh={handleRefresh}
        tintColor="#4A90E2"
        colors={['#4A90E2']}
        progressBackgroundColor="#F0F8FF"
        distanceToRefresh={1500}
      />
    );

    if (sorted.isGrouped) {
      const sectionsWithFilteredData = sorted.sections.map((section) => ({
        ...section,
        data: collapsedSections.has(section.title) ? [] : section.data,
      }));

      return (
        <SectionList
          sections={sectionsWithFilteredData}
          renderItem={renderFileItem}
          renderSectionHeader={renderSectionHeader}
          keyExtractor={(item) => fileKey(item)}
          showsVerticalScrollIndicator={false}
          contentContainerStyle={styles.listContainer}
          stickySectionHeadersEnabled={true}
          refreshControl={refreshControl}
          removeClippedSubviews={true}
          initialNumToRender={20}
          windowSize={11}
        />
      );
    }

    return (
      <FlatList
        data={sorted.data}
        renderItem={renderFileItem}
        keyExtractor={(item) => fileKey(item)}
        showsVerticalScrollIndicator={false}
        contentContainerStyle={styles.listContainer}
        refreshControl={refreshControl}
        removeClippedSubviews={true}
        initialNumToRender={20}
        windowSize={11}
      />
    );
  };

  const handleOpenFolderInfo = async () => {
    setShowFolderMenuModal(false);
    if (onFolderInfo) {
      await onFolderInfo();
    } else {
      showAlert('Folder Info', `Name: ${getCurrentFolderName()}\nPath: ${currentPath}`);
    }
  };

  const handleRenamePrompt = () => {
    setShowFolderMenuModal(false);
    showPrompt(
      'Rename Folder',
      'Enter new folder name:',
      (newName) => {
        if (newName && newName.trim()) {
          showAlert('Rename', `Rename functionality would go here: ${newName}`);
        }
      },
      'plain-text',
      getCurrentFolderName(),
    );
  };

  return (
    <View style={styles.container}>
      <View style={styles.headerContainer}>
        <View style={styles.safeAreaSpacer} />
        <View style={styles.header}>
          {multi.selectionMode ? (
            <>
              <TouchableOpacity
                style={styles.backButton}
                onPress={multi.exit}>
                <Text style={styles.backText}>Cancel</Text>
              </TouchableOpacity>
              <View style={styles.titleContainer}>
                <Text
                  style={styles.title}
                  numberOfLines={1}>
                  {`${selectionCount} selected`}
                </Text>
              </View>
              <TouchableOpacity
                style={styles.headerActions}
                onPress={handleSelectAll}>
                <Text style={styles.backText}>Select all</Text>
              </TouchableOpacity>
            </>
          ) : (
            <>
              <TouchableOpacity
                style={styles.headerActionButton}
                onPress={() => {
                  setShowSearchBar(!showSearchBar);
                  if (showSearchBar) setSearchQuery('');
                }}
                activeOpacity={0.7}>
                <Text style={styles.headerActionIcon}>{showSearchBar ? '\u2715' : '\uD83D\uDD0D'}</Text>
              </TouchableOpacity>
              <View style={styles.titleContainer}>
                <Text
                  style={styles.title}
                  numberOfLines={1}>
                  Cetapod SMB
                </Text>
              </View>
              <TouchableOpacity
                onPress={multi.enterEmpty}
                activeOpacity={0.7}>
                <Text style={[styles.backText, { fontWeight: '600' }]}>Select</Text>
              </TouchableOpacity>
            </>
          )}
        </View>
      </View>

      {showSearchBar && (
        <View style={styles.searchBarContainer}>
          <TextInput
            style={styles.searchInput}
            placeholder="Search files and folders..."
            value={searchQuery}
            onChangeText={setSearchQuery}
            autoFocus={true}
            returnKeyType="search"
            clearButtonMode="while-editing"
          />
        </View>
      )}

      <View style={styles.content}>{renderListBody()}</View>

      {multi.selectionMode ? (
        <View style={bulkStyles.bar}>
          <TouchableOpacity
            style={bulkStyles.btn}
            disabled={selectionCount === 0}
            onPress={() => runBulk('download')}>
            <Text style={[bulkStyles.btnText, selectionCount === 0 && bulkStyles.btnDisabled]}>Download</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={bulkStyles.btn}
            disabled={selectionCount === 0}
            onPress={() => runBulk('duplicate')}>
            <Text style={[bulkStyles.btnText, selectionCount === 0 && bulkStyles.btnDisabled]}>Duplicate</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={bulkStyles.btn}
            disabled={selectionCount === 0}
            onPress={() => runBulk('move')}>
            <Text style={[bulkStyles.btnText, selectionCount === 0 && bulkStyles.btnDisabled]}>Move</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={bulkStyles.btn}
            disabled={selectionCount === 0}
            onPress={() => runBulk('delete')}>
            <Text style={[bulkStyles.btnText, bulkStyles.danger, selectionCount === 0 && bulkStyles.btnDisabled]}>Delete</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <View style={styles.bottomBar}>
          <TouchableOpacity
            style={[styles.bottomBarButton, !navigation.canGoBack() && styles.bottomBarButtonDisabled]}
            onPress={() => {
              if (navigation.canGoBack()) {
                const parentPath = currentPath.split('/').slice(0, -1).join('/') || '/';
                navigation.goBack();
                onLoadFiles(parentPath);
              }
            }}
            disabled={!navigation.canGoBack()}>
            <Text style={[styles.bottomBarIcon, !navigation.canGoBack() && styles.bottomBarIconDisabled]}>{'\u2190'}</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={styles.bottomBarCenter}
            onPress={() => setShowFolderMenuModal(true)}>
            <Text
              style={styles.bottomBarFolderName}
              numberOfLines={1}>
              {getCurrentFolderName()}
            </Text>
            <Text style={styles.bottomBarDropdownIcon}>{'\u25B2'}</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={styles.bottomBarButton}
            onPress={() => setShowSortMenuModal(true)}>
            <Text style={styles.bottomBarIcon}>{'\u22EF'}</Text>
          </TouchableOpacity>
        </View>
      )}

      {/* FAB */}
      {!multi.selectionMode && (
        <TouchableOpacity
          style={[styles.fab, isLoading && styles.fabDisabled]}
          onPress={() => setShowFabMenuModal(true)}
          disabled={isLoading}
          activeOpacity={0.8}>
          <Text style={styles.fabIcon}>+</Text>
        </TouchableOpacity>
      )}

      <CreateFolderModal
        visible={showCreateModal}
        folderName={newFolderName}
        setFolderName={setNewFolderName}
        onCancel={() => setShowCreateModal(false)}
        onSubmit={handleCreateFolder}
      />

      <FolderMenuModal
        visible={showFolderMenuModal}
        onClose={() => setShowFolderMenuModal(false)}
        folderName={getCurrentFolderName()}
        onShowFolderInfo={handleOpenFolderInfo}
        onRenamePrompt={handleRenamePrompt}
      />

      <SortMenuModal
        visible={showSortMenuModal}
        onClose={() => setShowSortMenuModal(false)}
        sortBy={sortBy}
        sortOrder={sortOrder}
        onSelectSort={handleSortSelection}
        username={username}
        selectedShare={selectedShare}
        showHidden={showHidden}
        onToggleHidden={onToggleHidden}
        onChangeAccount={() => {
          setShowSortMenuModal(false);
          if (onLogout) onLogout();
        }}
      />

      <FabMenuModal
        visible={showFabMenuModal}
        onClose={() => setShowFabMenuModal(false)}
        onDismiss={() => {
          if (pendingFabActionRef.current) {
            const action = pendingFabActionRef.current;
            pendingFabActionRef.current = null;
            if (typeof action === 'function') action();
          }
        }}
        onPickNewFolder={() => {
          pendingFabActionRef.current = () => setShowCreateModal(true);
          setShowFabMenuModal(false);
        }}
        onPickUpload={() => {
          pendingFabActionRef.current = () => {
            if (onUploadFile) onUploadFile();
          };
          setShowFabMenuModal(false);
        }}
      />
    </View>
  );
};

const bulkStyles = StyleSheet.create({
  bar: {
    flexDirection: 'row',
    backgroundColor: '#FFFFFF',
    borderTopWidth: 1,
    borderTopColor: '#E6F2FF',
    paddingVertical: 12,
    paddingHorizontal: 8,
    justifyContent: 'space-around',
  },
  btn: { paddingHorizontal: 12, paddingVertical: 8 },
  btnText: { color: '#4A90E2', fontWeight: '600' },
  btnDisabled: { color: '#B8D4F1' },
  danger: { color: '#C62828' },
});

export default FilesScreen;
