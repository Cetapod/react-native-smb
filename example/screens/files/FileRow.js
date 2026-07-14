import React, { memo } from 'react';
import { StyleSheet, Text, TouchableOpacity, View } from 'react-native';

import { formatDate, formatFileSize, getFileIcon } from '../../utils/smbUtils';
import { filesStyles as styles } from './styles';

const folderCount = (item) => {
  if (typeof item.childCount === 'number' && item.childCount >= 0) return item.childCount;
  if (Array.isArray(item.children)) return item.children.length;
  return null;
};

const FileRow = ({ item, onOpen, onAction, selectionMode = false, isSelected = false, onToggleSelect }) => {
  const count = item.isDirectory ? folderCount(item) : null;

  const handlePress = () => {
    if (selectionMode) onToggleSelect?.(item);
    else if (item.isDirectory) onOpen(item);
    else onAction(item);
  };

  // Long-press always opens the action sheet (the "Select" button in the
  // header is now the entry point for multi-select mode).
  const handleLongPress = () => {
    if (selectionMode) onToggleSelect?.(item);
    else onAction(item);
  };

  return (
    <TouchableOpacity
      style={[styles.fileItem, selectionMode && isSelected && selectStyles.selectedRow]}
      onPress={handlePress}
      onLongPress={handleLongPress}>
      {selectionMode && <View style={[selectStyles.checkbox, isSelected && selectStyles.checkboxOn]}>{isSelected ? <Text style={selectStyles.checkboxMark}>{'\u2713'}</Text> : null}</View>}
      <View style={styles.fileIcon}>
        <Text style={styles.fileIconText}>{getFileIcon(item)}</Text>
      </View>
      <View style={styles.fileInfo}>
        <Text
          style={styles.fileName}
          numberOfLines={2}>
          {item.name}
        </Text>
        <View style={styles.fileDetails}>
          {item.isDirectory ? <Text style={styles.fileDetail}>{count === null ? 'Folder' : `${count} item${count === 1 ? '' : 's'}`}</Text> : <Text style={styles.fileDetail}>{formatFileSize(item.size || 0)}</Text>}
          {formatDate(item.modifiedAt) && <Text style={styles.fileDetail}>{'\u2022 ' + formatDate(item.modifiedAt)}</Text>}
        </View>
      </View>
      {!selectionMode && item.isDirectory && <Text style={styles.chevron}>{'\u203A'}</Text>}
    </TouchableOpacity>
  );
};

const selectStyles = StyleSheet.create({
  selectedRow: { backgroundColor: '#E6F2FF' },
  checkbox: {
    width: 22,
    height: 22,
    borderRadius: 11,
    borderWidth: 2,
    borderColor: '#4A90E2',
    marginRight: 12,
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: '#fff',
  },
  checkboxOn: { backgroundColor: '#4A90E2' },
  checkboxMark: { color: '#fff', fontSize: 14, fontWeight: '700' },
});

// Memoise so unrelated state changes higher in the tree (e.g. transfer
// progress ticks) don't re-render every row in a large directory.
export default memo(FileRow, (prev, next) => {
  if (prev.selectionMode !== next.selectionMode) return false;
  if (prev.isSelected !== next.isSelected) return false;
  if (prev.item === next.item) return true;
  return prev.item?.name === next.item?.name && prev.item?.size === next.item?.size && prev.item?.modifiedAt === next.item?.modifiedAt && prev.item?.isDirectory === next.item?.isDirectory && prev.item?.childCount === next.item?.childCount;
});
