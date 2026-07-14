import React, { useRef } from 'react';
import { Animated, Modal, PanResponder, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';

import { formatDate, formatFileSize, getFileIcon } from '../utils/smbUtils';

const QuickAction = ({ icon, label, onPress, destructive }) => (
  <TouchableOpacity
    style={styles.quickAction}
    onPress={onPress}
    activeOpacity={0.7}>
    <View style={[styles.quickIcon, destructive && styles.quickIconDestructive]}>
      <Text style={styles.quickIconText}>{icon}</Text>
    </View>
    <Text style={[styles.quickLabel, destructive && styles.quickLabelDestructive]}>{label}</Text>
  </TouchableOpacity>
);

const ListAction = ({ icon, title, onPress, destructive, danger }) => (
  <TouchableOpacity
    style={[styles.listItem, danger && styles.listItemDanger]}
    onPress={onPress}
    activeOpacity={0.6}>
    <Text style={styles.listIcon}>{icon}</Text>
    <Text style={[styles.listTitle, destructive && styles.listTitleDestructive]}>{title}</Text>
    {!destructive && <Text style={styles.listChevron}>{'\u203A'}</Text>}
  </TouchableOpacity>
);

const FileActionModal = ({ visible, file, onClose, onAction, showAlert }) => {
  const translateY = useRef(new Animated.Value(0)).current;
  const panResponderRef = useRef(null);

  if (!file) return null;

  if (!panResponderRef.current) {
    panResponderRef.current = PanResponder.create({
      onStartShouldSetPanResponder: () => false,
      onMoveShouldSetPanResponder: (_, gestureState) => gestureState.dy > 8 && Math.abs(gestureState.dx) < 20,
      onPanResponderMove: (_, gestureState) => {
        if (gestureState.dy > 0) translateY.setValue(gestureState.dy);
      },
      onPanResponderRelease: (_, gestureState) => {
        if (gestureState.dy > 100 || gestureState.vy > 0.5) {
          Animated.timing(translateY, { toValue: 500, duration: 200, useNativeDriver: true }).start(() => {
            translateY.setValue(0);
            onClose();
          });
        } else {
          Animated.spring(translateY, { toValue: 0, useNativeDriver: true, tension: 50, friction: 8 }).start();
        }
      },
    });
  }
  const panResponder = panResponderRef.current;

  const dispatch = (actionId) => {
    onClose();
    if (actionId === 'delete') {
      showAlert('Delete Item', `Are you sure you want to delete "${file.name}"?`, [
        { text: 'Cancel' },
        { text: 'Delete', style: 'destructive', onPress: () => onAction(actionId, file) },
      ]);
    } else {
      onAction(actionId, file);
    }
  };

  const isDir = !!file.isDirectory;
  const childCount = isDir ? (typeof file.childCount === 'number' ? file.childCount : Array.isArray(file.children) ? file.children.length : null) : null;
  const metaPrimary = isDir ? (childCount === null ? 'Folder' : `${childCount} item${childCount === 1 ? '' : 's'}`) : formatFileSize(file.size || 0);
  const metaSecondary = formatDate(file.modifiedAt);

  return (
    <Modal
      visible={visible}
      transparent
      animationType="slide"
      onRequestClose={onClose}>
      <TouchableOpacity
        style={styles.overlay}
        activeOpacity={1}
        onPress={onClose}>
        <Animated.View
          style={[styles.container, { transform: [{ translateY }] }]}>
          <TouchableOpacity
            activeOpacity={1}
            onPress={(e) => e.stopPropagation()}>
            {/* Drag handle (only this area listens for drag) */}
            <View
              style={styles.dragHandleContainer}
              {...panResponder.panHandlers}>
              <View style={styles.dragHandle} />
            </View>

            {/* Header: icon + filename + meta */}
            <View style={styles.header}>
              <View style={styles.headerIcon}>
                <Text style={styles.headerIconText}>{getFileIcon(file)}</Text>
              </View>
              <View style={styles.headerText}>
                <Text
                  style={styles.fileName}
                  numberOfLines={2}>
                  {file.name}
                </Text>
                <Text style={styles.fileMeta}>
                  {metaPrimary}
                  {metaSecondary ? ` · ${metaSecondary}` : ''}
                </Text>
              </View>
            </View>

            {/* Quick-action icon row */}
            <ScrollView
              horizontal
              showsHorizontalScrollIndicator={false}
              contentContainerStyle={styles.quickRow}>
              {!isDir && (
                <QuickAction
                  icon={'\u2B07'}
                  label="Download"
                  onPress={() => dispatch('download')}
                />
              )}
              <QuickAction
                icon={'\u29C9'}
                label="Duplicate"
                onPress={() => dispatch('duplicate')}
              />
              <QuickAction
                icon={'\u2398'}
                label="Copy"
                onPress={() => dispatch('copy')}
              />
              <QuickAction
                icon={'\u21AA'}
                label="Move"
                onPress={() => dispatch('move')}
              />
              <QuickAction
                icon={'\uD83D\uDDD1'}
                label="Delete"
                destructive
                onPress={() => dispatch('delete')}
              />
            </ScrollView>

            {/* Vertical list for less-frequent actions */}
            <View style={styles.list}>
              <ListAction
                icon={'\u24D8'}
                title="Get Info"
                onPress={() => dispatch('info')}
              />
              <ListAction
                icon={'\uD83D\uDD12'}
                title="View Permissions"
                onPress={() => dispatch('acl')}
              />
              <ListAction
                icon={'\u270E'}
                title="Rename"
                onPress={() => dispatch('rename')}
              />
            </View>
          </TouchableOpacity>
        </Animated.View>
      </TouchableOpacity>
    </Modal>
  );
};

const styles = StyleSheet.create({
  overlay: { flex: 1, backgroundColor: 'rgba(0,0,0,0.5)', justifyContent: 'flex-end' },
  container: {
    backgroundColor: '#FFFFFF',
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    maxHeight: '80%',
    paddingBottom: 24,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: -4 },
    shadowOpacity: 0.15,
    shadowRadius: 12,
    elevation: 8,
  },
  dragHandleContainer: { paddingVertical: 10, alignItems: 'center' },
  dragHandle: { width: 36, height: 4, backgroundColor: '#D1D5DB', borderRadius: 2 },

  header: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 20,
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#F3F4F6',
  },
  headerIcon: {
    width: 56,
    height: 56,
    borderRadius: 12,
    backgroundColor: '#E6F3FF',
    borderWidth: 1,
    borderColor: '#87CEEB',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 14,
  },
  headerIconText: { fontSize: 28 },
  headerText: { flex: 1 },
  fileName: { fontSize: 17, fontWeight: '600', color: '#111827', marginBottom: 4 },
  fileMeta: { fontSize: 13, color: '#6B7280' },

  quickRow: { paddingVertical: 14, paddingHorizontal: 12, gap: 8 },
  quickAction: { alignItems: 'center', width: 72, marginHorizontal: 4 },
  quickIcon: {
    width: 52,
    height: 52,
    borderRadius: 14,
    backgroundColor: '#EEF5FF',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 6,
  },
  quickIconDestructive: { backgroundColor: '#FEE2E2' },
  quickIconText: { fontSize: 22 },
  quickLabel: { fontSize: 12, color: '#1F2937', textAlign: 'center' },
  quickLabelDestructive: { color: '#B91C1C' },

  list: { borderTopWidth: 1, borderTopColor: '#F3F4F6' },
  listItem: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 20,
    paddingVertical: 14,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#F3F4F6',
  },
  listItemDanger: { backgroundColor: '#FEF2F2' },
  listIcon: { fontSize: 18, width: 28, color: '#374151' },
  listTitle: { flex: 1, fontSize: 16, color: '#111827', fontWeight: '500' },
  listTitleDestructive: { color: '#DC2626' },
  listChevron: { fontSize: 18, color: '#9CA3AF' },
});

export default FileActionModal;
