import { useEffect, useMemo, useRef, useState } from 'react';
import { ActivityIndicator, Modal, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { File } from 'expo-file-system';
import { Paths } from 'expo-file-system/next';
import { Image } from 'expo-image';

import { splitPath, useSubscribe, statusLabel } from '@cetapod/react-native-smb';

import { buildFilePath } from '../utils/connectionUtils';
import { formatFileSize, getFileIcon } from '../utils/smbUtils';
import { colors } from '../theme';

const LOG = '[file-preview]';

const IMAGE_EXTENSIONS = new Set(['jpg', 'jpeg', 'png', 'gif', 'webp', 'heic', 'heif', 'bmp', 'svg', 'avif']);

const isImageFile = (fileName) => {
  const { extension } = splitPath(fileName || '');
  return IMAGE_EXTENSIONS.has(extension.replace('.', '').toLowerCase());
};

export default function FilePreviewModal({ visible, file, currentPath, SMB, onClose }) {
  const [downloadTask, setDownloadTask] = useState(null);
  const [downloadReady, setDownloadReady] = useState(false);
  const [localPath, setLocalPath] = useState(null);
  const [error, setError] = useState(null);
  const inFlightRef = useRef(null);

  const { progress, status, snapshot } = useSubscribe(downloadTask);
  const progressValue = progress > 0 ? Math.min(Math.round(progress * 100), 100) : 0;
  const barFullWhileLoading = !downloadReady && progress >= 0.99;
  const [stuck, setStuck] = useState(false);

  const canPreviewImage = useMemo(() => file && isImageFile(file.name), [file]);

  useEffect(() => {
    if (!visible || !file) return undefined;

    setDownloadTask(null);
    setDownloadReady(false);
    setLocalPath(null);
    setError(null);
    setStuck(false);
    inFlightRef.current = null;

    const remotePath = buildFilePath(currentPath, file.name);
    const cachedPath = buildFilePath(Paths.cache.uri, file.name);

    const run = async () => {
      if (__DEV__) console.log(LOG, 'start', { remotePath, cachedPath });
      try {
        const task = SMB.downloadFile(remotePath, cachedPath);
        SMB.transferStore().hide(task.id);
        if (__DEV__) console.log(LOG, 'task-created', { taskId: task.id });
        setDownloadTask(task);

        await task.result();

        if (__DEV__) console.log(LOG, 'result-resolved', { taskId: task.id });
        setLocalPath(cachedPath);
        setDownloadReady(true);
      } catch (err) {
        if (__DEV__) console.warn(LOG, 'result-rejected', err?.message ?? String(err));
        setError(err?.message ?? String(err));
      }
    };

    inFlightRef.current = run();
    void inFlightRef.current;

    return () => {
      inFlightRef.current = null;
    };
  }, [visible, file, currentPath, SMB]);

  useEffect(() => {
    if (!barFullWhileLoading) {
      setStuck(false);
      return undefined;
    }
    if (__DEV__) {
      console.log(LOG, 'progress-full', {
        downloadReady,
        progress,
        status: statusLabel(status),
        taskId: snapshot?.taskId,
      });
    }
    const timer = setTimeout(() => {
      if (__DEV__) console.warn(LOG, 'STUCK progress full but preview not ready');
      setStuck(true);
    }, 2000);
    return () => clearTimeout(timer);
  }, [barFullWhileLoading, downloadReady, progress, status, snapshot?.taskId]);

  if (!file) return null;

  const renderContent = () => {
    if (error) {
      return <Text style={styles.errorText}>{error}</Text>;
    }
    if (!downloadReady) {
      return (
        <>
          <ActivityIndicator size="large" color={colors.primary} />
          <Text style={styles.loadingText}>Loading preview…</Text>
        </>
      );
    }
    if (canPreviewImage && localPath) {
      return <Image source={localPath} contentFit="contain" style={styles.previewImage} />;
    }
    return (
      <View style={styles.unsupported}>
        <Text style={styles.unsupportedIcon}>{getFileIcon(file)}</Text>
        <Text style={styles.unsupportedTitle}>Preview not available</Text>
        <Text style={styles.unsupportedName} numberOfLines={2}>
          {file.name}
        </Text>
        <Text style={styles.unsupportedMeta}>{formatFileSize(file.size || 0)}</Text>
        {localPath && new File(localPath).type ? (
          <Text style={styles.unsupportedMeta}>{new File(localPath).type}</Text>
        ) : null}
      </View>
    );
  };

  return (
    <Modal visible={visible} animationType="slide" onRequestClose={onClose}>
      <View style={styles.container}>
        <View style={styles.header}>
          <Text style={styles.title} numberOfLines={1}>
            {file.name}
          </Text>
          <TouchableOpacity onPress={onClose} style={styles.closeBtn}>
            <Text style={styles.closeText}>Close</Text>
          </TouchableOpacity>
        </View>

        <View style={styles.body}>{renderContent()}</View>

        {__DEV__ && !downloadReady ? (
          <View style={styles.debug}>
            <Text style={styles.debugLine}>downloadReady: {downloadReady ? 'true' : 'false'}</Text>
            <Text style={styles.debugLine}>subscribe status: {statusLabel(status)}</Text>
            <Text style={styles.debugLine}>progress: {progressValue}%</Text>
            {stuck ? <Text style={styles.stuck}>STUCK — bar full but result() not resolved</Text> : null}
          </View>
        ) : null}

        {!downloadReady ? (
          <View style={styles.progressTrack}>
            <View style={[styles.progressFill, { width: `${progressValue}%` }]} />
          </View>
        ) : null}
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#111827' },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingTop: 56,
    paddingHorizontal: 16,
    paddingBottom: 12,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: 'rgba(255,255,255,0.12)',
  },
  title: { flex: 1, fontSize: 16, fontWeight: '600', color: '#F9FAFB' },
  closeBtn: { paddingVertical: 8, paddingHorizontal: 12 },
  closeText: { color: colors.primaryLight, fontWeight: '600' },
  body: { flex: 1, alignItems: 'center', justifyContent: 'center' },
  previewImage: { width: '100%', height: '100%' },
  loadingText: { marginTop: 12, fontSize: 15, color: '#D1D5DB' },
  errorText: { fontSize: 15, color: '#FCA5A5', textAlign: 'center', paddingHorizontal: 24 },
  unsupported: { alignItems: 'center', paddingHorizontal: 24, gap: 8 },
  unsupportedIcon: { fontSize: 56, marginBottom: 8 },
  unsupportedTitle: { fontSize: 18, fontWeight: '600', color: '#F9FAFB' },
  unsupportedName: { fontSize: 15, color: '#D1D5DB', textAlign: 'center' },
  unsupportedMeta: { fontSize: 13, color: '#9CA3AF' },
  debug: {
    marginHorizontal: 16,
    marginBottom: 12,
    padding: 12,
    borderRadius: 10,
    backgroundColor: 'rgba(0,0,0,0.35)',
    gap: 4,
  },
  debugLine: { color: '#E5E7EB', fontSize: 12, fontFamily: 'Menlo' },
  stuck: { color: '#FCA5A5', fontSize: 12, fontWeight: '700', marginTop: 4 },
  progressTrack: {
    height: 3,
    backgroundColor: 'rgba(255,255,255,0.12)',
    overflow: 'hidden',
  },
  progressFill: { height: '100%', backgroundColor: colors.primary },
});
