import React from 'react';
import { Dimensions, Modal, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';

import { formatDate, formatFileSize, getFileIcon } from '../utils/smbUtils';

const FileInfoModal = ({ visible, fileInfo, onClose }) => {
  if (!fileInfo) {
    return null;
  }

  const renderInfoRow = (label, value, valueStyle = {}) => (
    <View style={styles.infoRow}>
      <Text style={styles.label}>{label}:</Text>
      <Text
        style={[styles.value, valueStyle]}
        numberOfLines={0}>
        {value}
      </Text>
    </View>
  );

  return (
    <Modal
      visible={visible}
      animationType="slide"
      transparent={true}
      onRequestClose={onClose}>
      <View style={styles.modalOverlay}>
        <View style={styles.container}>
          {/* Header */}
          <View style={styles.header}>
            <View style={styles.titleContainer}>
              <Text
                style={styles.title}
                numberOfLines={2}>
                Details
              </Text>
            </View>
            <TouchableOpacity
              style={styles.closeButton}
              onPress={onClose}>
              <Text style={styles.closeButtonText}>Done</Text>
            </TouchableOpacity>
          </View>

          {/* Content */}
          <ScrollView
            style={styles.content}
            showsVerticalScrollIndicator={false}>
            <View style={styles.mainCard}>
              {/* File Icon and Basic Info */}
              <View style={styles.fileHeader}>
                <View style={styles.fileIconContainer}>
                  <Text style={styles.largeFileIcon}>{getFileIcon(fileInfo)}</Text>
                </View>
                <View style={styles.fileBasicInfo}>
                  <Text
                    style={styles.fileName}
                    numberOfLines={2}>
                    {fileInfo.name}
                  </Text>
                  <Text style={styles.fileType}>{fileInfo.isDirectory ? 'Directory' : 'File'}</Text>
                  <Text style={styles.fileSize}>{fileInfo.isDirectory ? `${fileInfo.children.length || 0} item${(fileInfo.children.length || 0) !== 1 ? 's' : ''}` : formatFileSize(fileInfo.size || 0)}</Text>
                </View>
              </View>

              {/* Divider */}
              <View style={styles.divider} />

              {/* All Information in One Flow */}
              {renderInfoRow('Full Path', fileInfo.path, styles.pathValue)}
              {renderInfoRow('Last Modified', formatDate(fileInfo.modifiedAt))}
              {renderInfoRow('Date Created', formatDate(fileInfo.createdAt))}
              {renderInfoRow('Last Accessed', formatDate(fileInfo.accessedAt))}
              {renderInfoRow('Status Changed', formatDate(fileInfo.changedAt))}
            </View>
          </ScrollView>
        </View>
      </View>
    </Modal>
  );
};

const styles = StyleSheet.create({
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0, 0, 0, 0.4)',
    justifyContent: 'flex-end',
  },
  container: {
    backgroundColor: '#FFFFFF',
    borderTopLeftRadius: 20,
    borderTopRightRadius: 20,
    maxHeight: '80%',
    minHeight: '55%',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: -4 },
    shadowOpacity: 0.25,
    shadowRadius: 20,
    elevation: 10,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 20,
    paddingVertical: 20,
    backgroundColor: '#FFFFFF',
    borderTopLeftRadius: 20,
    borderTopRightRadius: 20,
    borderBottomWidth: 1,
    borderBottomColor: '#E6F2FF',
  },
  titleContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
    marginRight: 16,
  },
  fileIcon: {
    fontSize: 24,
    marginRight: 12,
  },
  title: {
    fontSize: 18,
    fontWeight: '600',
    color: '#4A6B8A',
    flex: 1,
    marginLeft: 12,
  },
  closeButton: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    backgroundColor: '#4A90E2',
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: '#4A90E2',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.2,
    shadowRadius: 4,
    elevation: 3,
  },
  closeButtonText: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: '600',
  },
  content: {
    flex: 1,
    paddingHorizontal: 20,
    paddingTop: 10,
    paddingBottom: 20,
  },
  mainCard: {
    backgroundColor: '#F8FBFF',
    borderRadius: 16,
    padding: 20,
    borderWidth: 1,
    borderColor: '#E6F2FF',
    shadowColor: '#4A90E2',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.08,
    shadowRadius: 8,
    elevation: 2,
  },
  fileHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 20,
  },
  fileIconContainer: {
    width: 60,
    height: 60,
    backgroundColor: '#F0F8FF',
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 16,
    borderWidth: 1,
    borderColor: '#D6E9F7',
  },
  largeFileIcon: {
    fontSize: 32,
  },
  fileBasicInfo: {
    flex: 1,
  },
  fileName: {
    fontSize: 20,
    fontWeight: '700',
    color: '#1E3A5F',
    marginBottom: 4,
  },
  fileType: {
    fontSize: 14,
    color: '#4A6B8A',
    fontWeight: '500',
    marginBottom: 2,
  },
  fileSize: {
    fontSize: 14,
    color: '#7BA8D1',
    fontWeight: '400',
  },
  divider: {
    height: 1,
    backgroundColor: '#E6F2FF',
    marginVertical: 16,
  },
  infoRow: {
    flexDirection: 'row',
    marginBottom: 12,
    alignItems: 'flex-start',
  },
  label: {
    fontSize: 14,
    fontWeight: '600',
    color: '#4A6B8A',
    width: 110,
    marginRight: 12,
  },
  value: {
    fontSize: 14,
    color: '#1E3A5F',
    flex: 1,
    lineHeight: 20,
  },
  pathValue: {
    fontFamily: 'monospace',
    fontSize: 12,
    backgroundColor: '#F0F8FF',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#E6F2FF',
  },
});

export default FileInfoModal;
