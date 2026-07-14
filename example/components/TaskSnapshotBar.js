import { StyleSheet, Text, TouchableOpacity, View } from 'react-native';

import { useSubscribe, statusLabel } from '@cetapod/react-native-smb';

const TaskSnapshotBar = ({ task, label = 'Download' }) => {
  const { progress, status, cancel, snapshot } = useSubscribe(task);

  if (!task) return null;

  const pct = snapshot?.determinate ? Math.round(progress * 100) : null;

  return (
    <View style={styles.bar}>
      <Text style={styles.title}>{label}</Text>
      <Text style={styles.status}>{statusLabel(status)}</Text>
      {pct !== null && <Text style={styles.progress}>{pct}%</Text>}
      {snapshot?.bytesPerSecond > 0 && (
        <Text style={styles.meta}>
          {(snapshot.bytesPerSecond / 1024).toFixed(0)} KB/s
        </Text>
      )}
      <TouchableOpacity onPress={cancel} style={styles.cancelBtn}>
        <Text style={styles.cancelText}>✕</Text>
      </TouchableOpacity>
    </View>
  );
};

const styles = StyleSheet.create({
  bar: {
    position: 'absolute',
    left: 12,
    right: 12,
    bottom: 88,
    padding: 12,
    borderRadius: 10,
    backgroundColor: 'rgba(17, 24, 39, 0.92)',
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  title: {
    color: '#F9FAFB',
    fontWeight: '600',
    flex: 1,
  },
  status: {
    color: '#93C5FD',
    fontSize: 13,
  },
  progress: {
    color: '#F9FAFB',
    fontSize: 13,
    fontWeight: '600',
  },
  meta: {
    color: '#9CA3AF',
    fontSize: 12,
  },
  cancelBtn: {
    width: 28,
    height: 28,
    borderRadius: 14,
    backgroundColor: 'rgba(239, 68, 68, 0.25)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  cancelText: {
    color: '#FCA5A5',
    fontSize: 14,
    fontWeight: '700',
  },
});

export default TaskSnapshotBar;