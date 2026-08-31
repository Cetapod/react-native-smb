import React, { useEffect, useMemo, useRef, useState } from 'react';
import {
  Alert,
  Animated,
  Dimensions,
  PanResponder,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';

import { SlotState, operationLabel } from '@cetapod/react-native-smb';

import { useSmb } from '../contexts/SmbContext';

const PollIntervalMs = 500;
const FetchTimeoutMs = 800;

function slotStateLabel(state) {
  switch (state) {
    case SlotState.Assigned:
      return 'busy';
    case SlotState.Activating:
      return 'activating';
    default:
      return 'idle';
  }
}

function shortTaskId(taskId) {
  if (!taskId) return '—';
  const tail = taskId.split('_').pop() ?? taskId;
  return tail.length > 10 ? `…${tail.slice(-8)}` : tail;
}

function slotTone(state) {
  switch (state) {
    case SlotState.Assigned:
      return styles.slotBusy;
    case SlotState.Activating:
      return styles.slotActivating;
    default:
      return styles.slotIdle;
  }
}

const PoolDebugPanel = () => {
  const { smb } = useSmb();
  const [info, setInfo] = useState({ poolSize: 0, slots: [] });
  const [collapsed, setCollapsed] = useState(true);
  const [session, setSession] = useState({ initialized: false, connected: false });

  const inFlightRef = useRef(false);
  const prevInfoRef = useRef(null);
  const subscriptionRef = useRef(null);

  useEffect(() => {
    if (!smb) return undefined;

    let mounted = true;

    const onSnapshot = (next) => {
      try {
        const normalised =
          next && Array.isArray(next.slots) ? next : { poolSize: 0, slots: [] };
        const infoStr = JSON.stringify(normalised);
        if (infoStr !== prevInfoRef.current) {
          prevInfoRef.current = infoStr;
          if (mounted) setInfo(normalised);
        }
      } catch {
        // ignore malformed snapshots
      }
    };

    const refreshSession = () => {
      if (!mounted || !smb) return;
      try {
        setSession({
          initialized: smb.isInitialized?.() ?? false,
          connected: smb.isConnected?.() ?? false,
        });
      } catch {
        // ignore
      }
    };

    const trySubscribe = () => {
      if (!smb.subscribePoolInfo) return false;
      try {
        const id = smb.subscribePoolInfo(onSnapshot);
        if (id) {
          subscriptionRef.current = id;
          return true;
        }
      } catch {
        // fall back to polling
      }
      return false;
    };

    let pollId = null;
    const startPolling = () => {
      const fetch = async () => {
        if (!smb.getPoolInfo) return;
        if (inFlightRef.current) return;
        inFlightRef.current = true;
        try {
          const timeout = new Promise((res) => setTimeout(() => res(null), FetchTimeoutMs));
          const next = await Promise.race([Promise.resolve(smb.getPoolInfo()), timeout]);
          if (next) onSnapshot(next);
        } catch {
          // ignore
        } finally {
          inFlightRef.current = false;
        }
      };
      fetch();
      pollId = setInterval(fetch, PollIntervalMs);
    };

    refreshSession();
    const sessionId = setInterval(refreshSession, PollIntervalMs);

    const subscribed = trySubscribe();
    if (!subscribed) startPolling();

    return () => {
      mounted = false;
      if (subscriptionRef.current && smb.unsubscribePoolInfo) {
        try {
          smb.unsubscribePoolInfo(subscriptionRef.current);
        } catch {
          // ignore
        }
        subscriptionRef.current = null;
      }
      if (pollId) clearInterval(pollId);
      clearInterval(sessionId);
    };
  }, [smb]);

  const pan = useRef(new Animated.ValueXY({ x: 0, y: 0 })).current;
  const panOriginRef = useRef({ x: 0, y: 0 });

  const panResponder = useRef(
    PanResponder.create({
      onMoveShouldSetPanResponder: (_, gs) => Math.abs(gs.dx) > 6 || Math.abs(gs.dy) > 6,
      onPanResponderGrant: () => {
        pan.setOffset(panOriginRef.current);
        pan.setValue({ x: 0, y: 0 });
      },
      onPanResponderMove: Animated.event([null, { dx: pan.x, dy: pan.y }], { useNativeDriver: false }),
      onPanResponderRelease: (_, gs) => {
        panOriginRef.current = {
          x: panOriginRef.current.x + gs.dx,
          y: panOriginRef.current.y + gs.dy,
        };
        pan.flattenOffset();
        pan.setValue({ x: 0, y: 0 });
      },
    }),
  ).current;

  const handleResetPool = () => {
    Alert.alert(
      'Reset Pool',
      'Force-disconnect every slot and clear cached config. Running transfers will be cancelled.',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Reset',
          style: 'destructive',
          onPress: () => {
            try {
              smb?.resetPool?.();
            } catch (e) {
              console.warn('resetPool failed', e?.message);
            }
          },
        },
      ],
    );
  };

  const { poolSize, slots } = info;

  const stats = useMemo(() => {
    const busy = slots.filter(
      (s) => s.state === SlotState.Assigned || s.state === SlotState.Activating,
    ).length;
    const connected = slots.filter((s) => s.isConnected).length;
    const total = poolSize || slots.length;
    return { busy, connected, total };
  }, [poolSize, slots]);

  if (!smb) return null;

  const fabLabel = collapsed
    ? `Pool ${stats.busy}/${stats.total || '?'}`
    : 'Hide';

  return (
    <Animated.View
      style={[styles.container, { transform: pan.getTranslateTransform() }]}
      pointerEvents="box-none">
      <TouchableOpacity
        style={styles.fab}
        onPress={() => setCollapsed((v) => !v)}
        activeOpacity={0.85}>
        <Text style={styles.fabText}>{fabLabel}</Text>
      </TouchableOpacity>

      {!collapsed && (
        <View style={styles.panel}>
          <View style={styles.headerRow} {...panResponder.panHandlers}>
            <View style={styles.headerLeft}>
              <Text style={styles.header}>Pool</Text>
              <Text style={styles.headerMeta}>
                {stats.busy} busy · {stats.connected} conn · {stats.total} slots
              </Text>
              <Text style={styles.headerMeta}>
                {session.initialized ? 'init ✓' : 'init —'}
                {' · '}
                {session.connected ? 'share ✓' : 'share —'}
              </Text>
            </View>
            <TouchableOpacity onPress={handleResetPool} style={styles.resetBtn}>
              <Text style={styles.resetBtnText}>Reset</Text>
            </TouchableOpacity>
          </View>

          {slots.length === 0 ? (
            <Text style={styles.emptyText}>No slot snapshot yet.</Text>
          ) : (
            slots.map((slot) => {
              const op =
                slot.state === SlotState.Idle ? '—' : operationLabel(slot.kind);
              return (
                <View key={slot.index} style={[styles.slotRow, slotTone(slot.state)]}>
                  <Text style={styles.slotIndex}>{slot.index}</Text>
                  <View style={styles.slotBody}>
                    <Text style={styles.slotPrimary}>
                      {slotStateLabel(slot.state)}
                      {slot.interactiveOnly ? ' · primary' : ''}
                    </Text>
                    <Text style={styles.slotSecondary} numberOfLines={1}>
                      {op}
                      {slot.state !== SlotState.Idle ? ` · ${shortTaskId(slot.taskId)}` : ''}
                    </Text>
                  </View>
                  <Text style={styles.slotFlag}>{slot.isConnected ? '●' : '○'}</Text>
                </View>
              );
            })
          )}
        </View>
      )}
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    right: 10,
    top: 56,
    zIndex: 9999,
    alignItems: 'flex-end',
    maxWidth: Dimensions.get('window').width - 20,
  },
  fab: {
    backgroundColor: 'rgba(17, 24, 39, 0.92)',
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 14,
    marginBottom: 6,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: 'rgba(255,255,255,0.12)',
  },
  fabText: { color: '#F9FAFB', fontSize: 12, fontWeight: '600' },
  panel: {
    backgroundColor: 'rgba(17, 24, 39, 0.94)',
    padding: 8,
    borderRadius: 10,
    width: 210,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: 'rgba(255,255,255,0.1)',
    gap: 4,
  },
  headerRow: {
    flexDirection: 'row',
    alignItems: 'flex-start',
    justifyContent: 'space-between',
    marginBottom: 4,
    paddingBottom: 4,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: 'rgba(255,255,255,0.08)',
  },
  headerLeft: { flex: 1, paddingRight: 6 },
  header: { color: '#F9FAFB', fontSize: 12, fontWeight: '700' },
  headerMeta: { color: '#9CA3AF', fontSize: 10, marginTop: 2 },
  resetBtn: {
    backgroundColor: 'rgba(239, 68, 68, 0.22)',
    paddingHorizontal: 8,
    paddingVertical: 3,
    borderRadius: 8,
  },
  resetBtnText: { color: '#FCA5A5', fontSize: 10, fontWeight: '700' },
  emptyText: { color: '#9CA3AF', fontSize: 11, paddingVertical: 4 },
  slotRow: {
    flexDirection: 'row',
    alignItems: 'center',
    borderRadius: 6,
    paddingHorizontal: 6,
    paddingVertical: 5,
    gap: 6,
  },
  slotIdle: { backgroundColor: 'rgba(34, 197, 94, 0.12)' },
  slotBusy: { backgroundColor: 'rgba(239, 68, 68, 0.18)' },
  slotActivating: { backgroundColor: 'rgba(245, 158, 11, 0.18)' },
  slotIndex: { color: '#E5E7EB', fontSize: 11, fontWeight: '700', width: 12 },
  slotBody: { flex: 1, minWidth: 0 },
  slotPrimary: { color: '#F3F4F6', fontSize: 11, fontWeight: '600' },
  slotSecondary: { color: '#9CA3AF', fontSize: 10, marginTop: 1 },
  slotFlag: { color: '#93C5FD', fontSize: 10, width: 10, textAlign: 'center' },
});

export default PoolDebugPanel;
