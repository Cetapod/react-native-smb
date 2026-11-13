import { useEffect, useMemo, useRef, useState } from 'react';
import { ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import * as Sharing from 'expo-sharing';
import { File } from 'expo-file-system';

import {
  SmbOperatorKind,
  TaskStatus,
  formatFileSize,
  isTerminalStatus,
  operationLabel,
  useTransferActions,
  useTransfers,
} from '@cetapod/react-native-smb';

import { useSmb } from '../contexts/SmbContext';

const SPEED_DEBOUNCE_MS = 1500;

const BADGE = {
  [TaskStatus.Running]: { text: 'Active', color: '#9EC5FF' },
  [TaskStatus.Pending]: { text: 'Waiting', color: '#C5B89A' },
  [TaskStatus.Success]: { text: 'Done', color: '#7BE495' },
  [TaskStatus.Error]: { text: 'Failed', color: '#FF6B6B' },
  [TaskStatus.Cancelled]: { text: 'Cancelled', color: '#BDBDBD' },
};

const unsettledRank = (s) =>
  s === TaskStatus.Running ? 0 : s === TaskStatus.Pending ? 1 : s === TaskStatus.Idle ? 2 : 3;

const formatDuration = (sec) => {
  if (!isFinite(sec) || sec < 0) return '--';
  if (sec < 60) return `${Math.ceil(sec)}s`;
  const m = Math.floor(sec / 60);
  const s = Math.ceil(sec % 60);
  if (m < 60) return `${m}m ${s}s`;
  return `${Math.floor(m / 60)}h ${m % 60}m`;
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
  return { speed, eta: Math.max(0, (t.totalBytes - done) / speed) };
};

const fileName = (t) => (t.destinationPath || t.sourcePath || '').split('/').pop() || '';

const shareFile = async (uri) => {
  if (!uri) return;
  try {
    const file = new File(uri);
    if (file.exists) await Sharing.shareAsync(uri);
  } catch {}
};

function TaskRow({ task, onCancel, onRemove }) {
  const badge = BADGE[task.status] ?? BADGE[TaskStatus.Pending];
  const pct = Math.round(task.progress * 100);
  const now = Date.now();
  const rate = task.status === TaskStatus.Running ? computeRate(task, now) : null;
  const speedSig = `${task.bytesPerSecond}:${task.bytesDone}:${task.progress}`;
  const [speedBps, setSpeedBps] = useState(() => rate?.speed ?? 0);
  const timer = useRef(null);

  useEffect(() => {
    if (task.status !== TaskStatus.Running) {
      clearTimeout(timer.current);
      return undefined;
    }
    clearTimeout(timer.current);
    timer.current = setTimeout(() => {
      const r = computeRate(task, Date.now());
      if (r?.speed > 0) setSpeedBps(r.speed);
    }, SPEED_DEBOUNCE_MS);
    return () => clearTimeout(timer.current);
  }, [task.taskId, speedSig, task.status]);

  let meta = badge.text || `${pct}%`;
  if (rate) {
    const speed = speedBps > 0 ? speedBps : rate.speed;
    meta = `${pct}% · ${formatFileSize(speed)}/s · ${formatDuration(rate.eta)} left`;
  }

  const isDone = task.status === TaskStatus.Success;
  const isDownload = task.kind === SmbOperatorKind.DownloadFile;
  const shareUri = isDownload ? task.destinationPath : '';
  const active = !isTerminalStatus(task.status);

  return (
    <TouchableOpacity
      activeOpacity={isDone && shareUri ? 0.7 : 1}
      onPress={() => isDone && shareUri && shareFile(shareUri)}
      style={styles.row}>
      <View style={styles.rowBody}>
        <Text numberOfLines={1} style={styles.rowTitle}>
          {operationLabel(task.kind)} {fileName(task)}
        </Text>
        <View style={styles.track}>
          <View style={[styles.fill, { width: `${Math.min(100, Math.max(0, pct))}%`, backgroundColor: badge.color }]} />
        </View>
        <Text style={[styles.meta, { color: badge.color }]}>{meta}</Text>
        {task.errorMessage ? (
          <Text numberOfLines={2} style={styles.err}>
            {task.errorMessage}
          </Text>
        ) : null}
      </View>
      <TouchableOpacity style={styles.rowBtn} onPress={() => (active ? onCancel(task.taskId) : onRemove(task.taskId))}>
        <Text style={styles.rowBtnText}>{active ? 'Cancel' : 'Clear'}</Text>
      </TouchableOpacity>
    </TouchableOpacity>
  );
}

export default function TransferTrayPanel() {
  const { smb } = useSmb();
  const transferTasks = useTransfers(smb);
  const { clearCompleted, hide } = useTransferActions(smb);

  const orderRef = useRef(new Map());
  const orderNext = useRef(0);
  const [expanded, setExpanded] = useState(false);
  const [openActive, setOpenActive] = useState(true);
  const [openDone, setOpenDone] = useState(true);

  const tasks = useMemo(() => {
    for (const t of transferTasks) {
      if (!orderRef.current.has(t.taskId)) orderRef.current.set(t.taskId, orderNext.current++);
    }
    return transferTasks;
  }, [transferTasks]);

  const order = orderRef.current;
  const byUnsettled = (a, b) => unsettledRank(a.status) - unsettledRank(b.status) || order.get(a.taskId) - order.get(b.taskId);
  const bySettled = (a, b) => (b.endedAt || 0) - (a.endedAt || 0) || order.get(a.taskId) - order.get(b.taskId);

  const activeTasks = useMemo(
    () => tasks.filter((t) => !isTerminalStatus(t.status)).sort(byUnsettled),
    [tasks],
  );
  const settledTasks = useMemo(() => tasks.filter((t) => isTerminalStatus(t.status)).sort(bySettled), [tasks]);

  const inFlight = activeTasks.filter((t) => t.status === TaskStatus.Running || t.status === TaskStatus.Pending);
  const totalPct =
    inFlight.length === 0
      ? 100
      : Math.round(inFlight.reduce((n, t) => n + Math.round(t.progress * 100), 0) / inFlight.length);

  const shareUris = settledTasks
    .filter((t) => t.status === TaskStatus.Success && t.kind === SmbOperatorKind.DownloadFile && t.destinationPath)
    .map((t) => t.destinationPath);

  if (!smb || tasks.length === 0) return null;

  const n = (c) => `${c} transfer${c === 1 ? '' : 's'}`;
  const pill = activeTasks.length > 0 ? `${n(activeTasks.length)} · ${totalPct}%` : `${n(tasks.length)} complete`;

  const section = (title, items, open, toggle) =>
    items.length === 0 ? null : (
      <View style={styles.section}>
        <TouchableOpacity onPress={toggle} style={styles.secHead}>
          <Text style={styles.secChevron}>{open ? '\u25BE' : '\u25B8'}</Text>
          <Text style={styles.secTitle}>{title}</Text>
          <Text style={styles.secCount}>{items.length}</Text>
        </TouchableOpacity>
        {open ? items.map((t) => (
          <TaskRow
            key={t.taskId}
            task={t}
            onCancel={(id) => smb.cancelTask(id)}
            onRemove={hide}
          />
        )) : null}
      </View>
    );

  return (
    <View style={styles.wrap} pointerEvents="box-none">
      <TouchableOpacity style={styles.pill} onPress={() => setExpanded((v) => !v)}>
        <Text style={styles.pillText}>{pill}</Text>
        <Text style={styles.pillChevron}>{expanded ? '\u2193' : '\u2191'}</Text>
      </TouchableOpacity>
      {expanded && (
        <View style={styles.panel}>
          <View style={styles.header}>
            <Text style={styles.headerText}>Transfers</Text>
            <View style={styles.headerRow}>
              {activeTasks.length > 0 && (
                <TouchableOpacity onPress={() => smb.cancelTransferTasks()} style={styles.hdrBtn}>
                  <Text style={[styles.hdrBtnText, styles.danger]}>Cancel all</Text>
                </TouchableOpacity>
              )}
              {activeTasks.length === 0 && shareUris.length >= 2 && (
                <TouchableOpacity
                  onPress={async () => {
                    for (const u of shareUris) {
                      // eslint-disable-next-line no-await-in-loop
                      await shareFile(u);
                    }
                  }}
                  style={styles.hdrBtn}>
                  <Text style={[styles.hdrBtnText, styles.ok]}>Share all ({shareUris.length})</Text>
                </TouchableOpacity>
              )}
              {settledTasks.length > 0 && (
                <TouchableOpacity onPress={clearCompleted} style={styles.hdrBtn}>
                  <Text style={styles.hdrBtnText}>Clear done</Text>
                </TouchableOpacity>
              )}
            </View>
          </View>
          <ScrollView style={styles.list}>
            {section('In progress', activeTasks, openActive, () => setOpenActive((v) => !v))}
            {section('Finished', settledTasks, openDone, () => setOpenDone((v) => !v))}
          </ScrollView>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { position: 'absolute', right: 12, bottom: 96, width: 340, maxWidth: '92%', alignItems: 'flex-end' },
  pill: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1E3A5F',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 999,
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
    elevation: 6,
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 10,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255,255,255,0.12)',
  },
  headerText: { fontWeight: '700', color: '#fff' },
  headerRow: { flexDirection: 'row' },
  hdrBtn: { paddingHorizontal: 8, paddingVertical: 4, marginLeft: 4 },
  hdrBtnText: { color: '#CFE2FF', fontSize: 12, fontWeight: '600' },
  ok: { color: '#7BE495' },
  danger: { color: '#FF6B6B' },
  list: { paddingHorizontal: 8, paddingVertical: 4 },
  section: { marginBottom: 4 },
  secHead: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 8,
    paddingHorizontal: 6,
    marginTop: 4,
    borderRadius: 6,
    backgroundColor: 'rgba(255,255,255,0.06)',
  },
  secChevron: { color: 'rgba(255,255,255,0.7)', fontSize: 14, width: 16, marginRight: 4 },
  secTitle: { flex: 1, fontSize: 11, fontWeight: '700', color: 'rgba(255,255,255,0.85)', textTransform: 'uppercase' },
  secCount: { fontSize: 11, fontWeight: '700', color: '#CFE2FF' },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 8,
    paddingHorizontal: 6,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: 'rgba(255,255,255,0.1)',
  },
  rowBody: { flex: 1 },
  rowTitle: { fontSize: 13, color: '#fff', fontWeight: '500' },
  track: { height: 4, backgroundColor: 'rgba(255,255,255,0.18)', borderRadius: 2, marginTop: 4, overflow: 'hidden' },
  fill: { height: 4, borderRadius: 2 },
  meta: { fontSize: 11, marginTop: 3 },
  err: { fontSize: 11, color: '#FF8A8A', marginTop: 2 },
  rowBtn: { paddingHorizontal: 8, paddingVertical: 4, marginLeft: 8 },
  rowBtnText: { color: '#CFE2FF', fontSize: 12, fontWeight: '600' },
});
