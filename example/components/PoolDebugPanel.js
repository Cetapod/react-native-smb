import React, { useEffect, useRef, useState } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { Alert, Animated, Dimensions, PanResponder, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';

import { SlotState, operationLabel } from '@cetapod/react-native-smb';

import { useSmb } from '../contexts/SmbContext';

const PollIntervalMs = 500;
const FetchTimeoutMs = 800;
const POSITION_KEY = '@cetapod/poolDebug/pos/v1';

const PoolDebugPanel = () => {
  const { smb } = useSmb();
  const [info, setInfo] = useState({ poolSize: 0, slots: [] });
  const [collapsed, setCollapsed] = useState(true);

  const inFlightRef = useRef(false);
  const prevInfoRef = useRef(null);
  const subscriptionRef = useRef(null);

  useEffect(() => {
    let mounted = true;

    const onSnapshot = (next) => {
      try {
        const normalised = next && Array.isArray(next.slots) ? next : { poolSize: 0, slots: [] };
        const infoStr = JSON.stringify(normalised);
        if (infoStr !== prevInfoRef.current) {
          prevInfoRef.current = infoStr;
          if (mounted) setInfo(normalised);
        }
      } catch (e) {}
    };

    const trySubscribe = () => {
      if (!smb || !smb.subscribePoolInfo) return false;
      try {
        const id = smb.subscribePoolInfo(onSnapshot);
        if (id) {
          subscriptionRef.current = id;
          return true;
        }
      } catch (e) {}
      return false;
    };

    let pollId = null;
    const startPolling = () => {
      const fetch = async () => {
        if (!smb || !smb.getPoolInfo) return;
        if (inFlightRef.current) return;
        inFlightRef.current = true;
        try {
          const timeout = new Promise((res) => setTimeout(() => res(null), FetchTimeoutMs));
          const next = await Promise.race([Promise.resolve(smb.getPoolInfo()), timeout]);
          if (next) onSnapshot(next);
        } catch (e) {
        } finally {
          inFlightRef.current = false;
        }
      };
      fetch();
      pollId = setInterval(fetch, PollIntervalMs);
    };

    const subscribed = trySubscribe();
    if (!subscribed) startPolling();

    return () => {
      mounted = false;
      if (subscriptionRef.current && smb && smb.unsubscribePoolInfo) {
        try {
          smb.unsubscribePoolInfo(subscriptionRef.current);
        } catch (e) {}
        subscriptionRef.current = null;
      }
      if (pollId) clearInterval(pollId);
    };
  }, [smb]);

  const pan = useRef(new Animated.ValueXY({ x: 0, y: 0 })).current;
  const offsetRef = useRef({ x: 0, y: 0 });

  useEffect(() => {
    (async () => {
      try {
        const raw = await AsyncStorage.getItem(POSITION_KEY);
        if (raw) {
          const parsed = JSON.parse(raw);
          if (typeof parsed?.x === 'number' && typeof parsed?.y === 'number') {
            offsetRef.current = parsed;
            pan.setOffset(parsed);
            pan.setValue({ x: 0, y: 0 });
          }
        }
      } catch {}
    })();
  }, [pan]);

  const panResponder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => false,
      onMoveShouldSetPanResponder: (_, gs) => Math.abs(gs.dx) > 4 || Math.abs(gs.dy) > 4,
      onPanResponderGrant: () => {
        pan.setOffset(offsetRef.current);
        pan.setValue({ x: 0, y: 0 });
      },
      onPanResponderMove: Animated.event([null, { dx: pan.x, dy: pan.y }], { useNativeDriver: false }),
      onPanResponderRelease: (_, gs) => {
        const screen = Dimensions.get('window');
        const next = {
          x: Math.max(-screen.width + 80, Math.min(0, offsetRef.current.x + gs.dx)),
          y: Math.max(-screen.height + 200, Math.min(screen.height - 200, offsetRef.current.y + gs.dy)),
        };
        offsetRef.current = next;
        pan.flattenOffset();
        pan.setValue(next);
        AsyncStorage.setItem(POSITION_KEY, JSON.stringify(next)).catch(() => {});
      },
    }),
  ).current;

  const handleResetPool = () => {
    Alert.alert(
      'Reset Pool',
      'Force-disconnect every slot and forget cached config. Any running transfers will be cancelled. Continue?',
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

  return (
    <Animated.View
      style={[styles.container, { transform: pan.getTranslateTransform() }]}
      pointerEvents="box-none">
      <View {...panResponder.panHandlers}>
        <TouchableOpacity
          style={styles.fab}
          onPress={() => setCollapsed((v) => !v)}
          activeOpacity={0.8}>
          <Text style={styles.fabText}>{collapsed ? `Pool ${slots.length}/${poolSize || '?'}` : '\u25B2 hide'}</Text>
        </TouchableOpacity>
      </View>
      {!collapsed && (
        <View style={styles.panel}>
          <View style={styles.headerRow}>
            <Text style={styles.header}>
              Pool {slots.length}/{poolSize || '?'}
            </Text>
            <TouchableOpacity
              onPress={handleResetPool}
              style={styles.resetBtn}>
              <Text style={styles.resetBtnText}>Reset</Text>
            </TouchableOpacity>
          </View>
          {slots.length === 0 ? (
            <View style={styles.emptyCard}>
              <Text style={styles.emptyText}>Pool is empty</Text>
              <Text style={styles.emptySub}>Slots are created on demand. Perform any SMB operation (browse a folder, download a file, etc.) and the active slots will appear here.</Text>
            </View>
          ) : (
            <ScrollView
              contentContainerStyle={styles.row}
              showsHorizontalScrollIndicator={false}>
              {slots.map((s, i) => (
                <View
                  key={i}
                  style={[styles.slot, s.state === SlotState.Assigned ? styles.busy : styles.idle]}>
                  <Text style={styles.slotTitle}>Slot {s.index ?? i}</Text>
                  <Text style={styles.slotText}>{s.state === SlotState.Assigned ? 'IN USE' : 'idle'}</Text>
                  <Text style={styles.slotText}>State: {s.state}</Text>
                  <Text style={styles.slotText}>Interactive: {s.interactiveOnly ? 'yes' : 'no'}</Text>
                  <Text style={styles.slotText}>Conn: {s.isConnected ? 'yes' : 'no'}</Text>
                  <Text style={styles.slotText}>Share: {s.shareName || '-'}</Text>
                  <Text style={styles.slotText}>Task: {s.taskId || '-'}</Text>
                  <Text style={styles.slotText}>Op: {operationLabel(s.kind)}</Text>
                </View>
              ))}
            </ScrollView>
          )}
        </View>
      )}
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  container: { position: 'absolute', right: 10, top: 60, zIndex: 9999, alignItems: 'flex-end' },
  fab: {
    backgroundColor: 'rgba(30,30,30,0.85)',
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 16,
    marginBottom: 6,
  },
  fabText: { color: '#fff', fontSize: 12, fontWeight: '600' },
  panel: {
    backgroundColor: 'rgba(0,0,0,0.55)',
    padding: 6,
    borderRadius: 8,
    maxWidth: 220,
  },
  headerRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', paddingHorizontal: 4, marginBottom: 4 },
  header: { color: '#fff', fontSize: 12, fontWeight: '700' },
  resetBtn: { backgroundColor: 'rgba(220,80,80,0.95)', paddingHorizontal: 8, paddingVertical: 2, borderRadius: 10 },
  resetBtnText: { color: '#fff', fontSize: 11, fontWeight: '700' },
  row: { alignItems: 'center' },
  slot: { padding: 8, margin: 4, borderRadius: 6, minWidth: 180 },
  busy: { backgroundColor: 'rgba(255,80,80,0.95)' },
  idle: { backgroundColor: 'rgba(80,200,120,0.95)' },
  slotTitle: { fontWeight: '600', color: '#fff' },
  slotText: { color: '#fff', fontSize: 12 },
  emptyCard: {
    padding: 10,
    margin: 4,
    borderRadius: 6,
    backgroundColor: 'rgba(120,120,120,0.85)',
    minWidth: 200,
    maxWidth: 200,
  },
  emptyText: { color: '#fff', fontWeight: '600', marginBottom: 4 },
  emptySub: { color: '#eee', fontSize: 11, lineHeight: 14 },
});

export default PoolDebugPanel;
