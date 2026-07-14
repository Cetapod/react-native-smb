import { useEffect, useMemo, useRef, useState } from 'react';
import { ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import * as Sharing from 'expo-sharing';
import { File } from 'expo-file-system';

import {
  SmbOperatorKind,
  TaskStatus,
  operationLabel,
  useTransferActions,
  useTransfers,
} from '@cetapod/react-native-smb';

import { useSmb } from '../contexts/SmbContext';

const statusBadge = {
  running: { text: '', color: '#9EC5FF' },
  pending: { text: 'Waiting', color: '#C5B89A' },
  completed: { text: 'Done', color: '#7BE495' },
  failed: { text: 'Failed', color: '#FF6B6B' },
  cancelled: { text: 'Cancelled', color: '#BDBDBD' },
};

const shareFile = async (uri) => {
  if (!uri) return;
  try {
    const file = new File(uri);
    if (!file.exists) return;
    await Sharing.shareAsync(uri);
  } catch {}
};

const shareAll = async (uris) => {
  for (const uri of uris) {
    // eslint-disable-next-line no-await-in-loop
    await shareFile(uri);
  }
};

const formatBytes = (n) => {
  if (!n || n <= 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let i = 0;
  let v = n;
  while (v >= 1024 && i < units.length - 1) {
    v /= 1024;
    i++;
  }
  return `${v.toFixed(v >= 10 || i === 0 ? 0 : 1)} ${units[i]}`;
};

const formatDuration = (seconds) => {
  if (!isFinite(seconds) || seconds < 0) return '--';
  if (seconds < 60) return `${Math.ceil(seconds)}s`;
  const m = Math.floor(seconds / 60);
  const s = Math.ceil(seconds % 60);
  if (m < 60) return `${m}m ${s}s`;
  const h = Math.floor(m / 60);
  return `${h}h ${m % 60}m`;
};

const computeRate = (t, now) => {
  if (t.bytesPerSecond > 0 && t.etaSeconds >= 0) {
    return { speed: t.bytesPerSecond, eta: t.etaSeconds };
  }
  if (!t.totalBytes || !t.startedAt) return null;
  const elapsed = (now - t.startedAt) / 1000;
  if (elapsed < 0.4) return null;
  const done = t.bytesDone || t.progress * t.totalBytes;
  const speed = done / elapsed;
  if (!isFinite(speed) || speed <= 0) return null;
  const remaining = t.totalBytes - done;
  const eta = remaining > 0 ? remaining / speed : 0;
  return { speed, eta };
};

const getTaskDisplayName = (task) => {
  const path = task.destinationPath || task.sourcePath;
  if (path) {
    const parts = path.split('/');
    return parts[parts.length - 1];
  }
  return 'Unknown';
};

const badgeKey = (status) => {
  if (status === TaskStatus.Success) return 'completed';
  if (status === TaskStatus.Error) return 'failed';
  if (status === TaskStatus.Cancelled) return 'cancelled';
  if (status === TaskStatus.Running) return 'running';
  if (status === TaskStatus.Pending) return 'pending';
  return 'pending';
};

const TransferTrayPanel = () => {
  const { smb } = useSmb();
  const transferTasks = useTransfers(smb);
  const { clearCompleted, hide } = useTransferActions(smb);

  const displayOrderRef = useRef(new Map());
  const nextDisplayOrderRef = useRef(0);
  const tasks = useMemo(() => {
    for (const t of transferTasks) {
      if (!displayOrderRef.current.has(t.taskId)) {
        displayOrderRef.current.set(t.taskId, nextDisplayOrderRef.current++);
      }
    }
    return [...transferTasks].sort(
      (a, b) => displayOrderRef.current.get(a.taskId) - displayOrderRef.current.get(b.taskId),
    );
  }, [transferTasks]);
  const runningTasks = useMemo(
    () =>
      tasks.filter(
        (t) =>
          t.status === TaskStatus.Running || t.status === TaskStatus.Pending,
      ),
    [tasks],
  );
  const completedTasks = useMemo(
    () =>
      tasks.filter(
        (t) =>
          t.status === TaskStatus.Success ||
          t.status === TaskStatus.Error ||
          t.status === TaskStatus.Cancelled,
      ),
    [tasks],
  );
  const totalProgress = useMemo(() => {
    if (runningTasks.length === 0) return 100;
    const sum = runningTasks.reduce((acc, t) => acc + t.progress * 100, 0);
    return Math.round(sum / runningTasks.length);
  }, [runningTasks]);
  const shareableUris = useMemo(() => {
    return completedTasks
      .filter((t) => t.status === TaskStatus.Success && t.kind === SmbOperatorKind.DownloadFile && t.destinationPath)
      .map((t) => t.destinationPath);
  }, [completedTasks]);

  const cancelTask = (taskId) => smb?.cancelTask(taskId);
  const cancelAll = () => runningTasks.forEach((t) => smb?.cancelTask(t.taskId));
  const removeTask = (taskId) => hide(taskId);
  const [expanded, setExpanded] = useState(false);

  const [, setTick] = useState(0);
  useEffect(() => {
    if (runningTasks.length === 0) return undefined;
    const h = setInterval(() => setTick((x) => x + 1), 1000);
    return () => clearInterval(h);
  }, [runningTasks]);

  if (!smb || tasks.length === 0) return null;

  const showShareAll = runningTasks.length === 0 && shareableUris.length >= 2;
  const now = Date.now();

  return (
    <View
      style={styles.wrap}
      pointerEvents="box-none">
      <TouchableOpacity
        activeOpacity={0.85}
        onPress={() => setExpanded((v) => !v)}
        style={styles.pill}>
        <Text style={styles.pillText}>{runningTasks.length > 0 ? `${runningTasks.length} transfer${runningTasks.length === 1 ? '' : 's'} · ${totalProgress}%` : `${tasks.length} transfer${tasks.length === 1 ? '' : 's'} complete`}</Text>
        <Text style={styles.pillChevron}>{expanded ? '\u2193' : '\u2191'}</Text>
      </TouchableOpacity>

      {expanded && (
        <View style={styles.panel}>
          <View style={styles.header}>
            <Text style={styles.headerText}>Transfers</Text>
            <View style={{ flexDirection: 'row' }}>
              {runningTasks.length > 0 && (
                <TouchableOpacity
                  onPress={cancelAll}
                  style={styles.headerBtn}>
                  <Text style={[styles.headerBtnText, styles.headerBtnDestructive]}>Cancel all</Text>
                </TouchableOpacity>
              )}
              {showShareAll && (
                <TouchableOpacity
                  onPress={() => shareAll(shareableUris)}
                  style={styles.headerBtn}>
                  <Text style={[styles.headerBtnText, styles.headerBtnAccent]}>Share all ({shareableUris.length})</Text>
                </TouchableOpacity>
              )}
              {completedTasks.length > 0 && (
                <TouchableOpacity
                  onPress={clearCompleted}
                  style={styles.headerBtn}>
                  <Text style={styles.headerBtnText}>Clear done</Text>
                </TouchableOpacity>
              )}
            </View>
          </View>
          <ScrollView style={styles.list}>
            {tasks.map((t) => {
              const progress = Math.round(t.progress * 100);
              const badge = statusBadge[badgeKey(t.status)] || statusBadge.running;
              const isDone = t.status === TaskStatus.Success;
              const isDownload = t.kind === SmbOperatorKind.DownloadFile;
              const isRunning = t.status === TaskStatus.Running;
              const shareUri = isDownload ? t.destinationPath : '';
              const rate = isRunning ? computeRate(t, now) : null;

              let metaText = badge.text || `${progress}%`;
              if (isRunning && rate) {
                metaText = `${progress}% · ${formatBytes(rate.speed)}/s · ${formatDuration(rate.eta)} left`;
              }

              return (
                <TouchableOpacity
                  key={t.taskId}
                  activeOpacity={isDone && isDownload && shareUri ? 0.7 : 1}
                  onPress={() => {
                    if (isDone && isDownload && shareUri) shareFile(shareUri);
                  }}
                  style={styles.row}>
                  <View style={{ flex: 1 }}>
                    <Text
                      numberOfLines={1}
                      style={styles.rowTitle}>
                      {operationLabel(t.kind)} {getTaskDisplayName(t)}
                    </Text>
                    <View style={styles.progressTrack}>
                      <View style={[styles.progressFill, { width: `${Math.max(0, Math.min(100, progress))}%`, backgroundColor: badge.color }]} />
                    </View>
                    <Text style={[styles.rowMeta, { color: badge.color }]}>{metaText}</Text>
                    {t.errorMessage ? (
                      <Text
                        numberOfLines={2}
                        style={styles.rowError}>
                        {t.errorMessage}
                      </Text>
                    ) : null}
                  </View>
                  <TouchableOpacity
                    style={styles.rowAction}
                    onPress={() => (isRunning ? cancelTask(t.taskId) : removeTask(t.taskId))}>
                    <Text style={styles.rowActionText}>{isRunning ? 'Cancel' : 'Clear'}</Text>
                  </TouchableOpacity>
                </TouchableOpacity>
              );
            })}
          </ScrollView>
        </View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  wrap: {
    position: 'absolute',
    right: 12,
    bottom: 96,
    width: 340,
    maxWidth: '92%',
    alignItems: 'flex-end',
  },
  pill: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1E3A5F',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 999,
    shadowColor: '#000',
    shadowOpacity: 0.2,
    shadowRadius: 4,
    elevation: 4,
  },
  pillText: { color: '#fff', fontWeight: '600', marginRight: 6, fontSize: 12 },
  pillChevron: { color: '#fff', fontSize: 13 },
  panel: {
    marginTop: 8,
    backgroundColor: '#1E3A5F',
    borderRadius: 12,
    width: '100%',
    maxHeight: 360,
    shadowColor: '#000',
    shadowOpacity: 0.25,
    shadowRadius: 8,
    elevation: 6,
    overflow: 'hidden',
  },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingHorizontal: 12, paddingVertical: 10, borderBottomWidth: 1, borderBottomColor: 'rgba(255,255,255,0.12)' },
  headerText: { fontWeight: '700', color: '#FFFFFF' },
  headerBtn: { paddingHorizontal: 8, paddingVertical: 4, marginLeft: 4 },
  headerBtnText: { color: '#CFE2FF', fontSize: 12, fontWeight: '600' },
  headerBtnAccent: { color: '#7BE495' },
  headerBtnDestructive: { color: '#FF6B6B' },
  list: { paddingHorizontal: 8, paddingVertical: 4 },
  row: { flexDirection: 'row', alignItems: 'center', paddingVertical: 8, paddingHorizontal: 6, borderBottomWidth: StyleSheet.hairlineWidth, borderBottomColor: 'rgba(255,255,255,0.1)' },
  rowTitle: { fontSize: 13, color: '#FFFFFF', fontWeight: '500' },
  progressTrack: { height: 4, backgroundColor: 'rgba(255,255,255,0.18)', borderRadius: 2, marginTop: 4, overflow: 'hidden' },
  progressFill: { height: 4, borderRadius: 2 },
  rowMeta: { fontSize: 11, marginTop: 3 },
  rowError: { fontSize: 11, color: '#FF8A8A', marginTop: 2 },
  rowAction: { paddingHorizontal: 8, paddingVertical: 4, marginLeft: 8 },
  rowActionText: { color: '#CFE2FF', fontSize: 12, fontWeight: '600' },
});

export default TransferTrayPanel;