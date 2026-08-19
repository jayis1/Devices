import React from 'react';
import { View, Text, Button, StyleSheet } from 'react-native';
import { useCycleGuard } from '../services/CycleGuardContext';

const LOCK_LABELS: Record<number, string> = {
  0: 'Disarmed', 1: 'Armed', 2: 'Tamper', 3: 'Alarm', 4: 'Tracking',
};
const LOCK_COLORS: Record<number, string> = {
  0: '#7f8c8d', 1: '#2ecc71', 2: '#f39c12', 3: '#e74c3c', 4: '#e74c3c',
};

export default function LockScreen() {
  const { lock, armLock, disarmLock, connected } = useCycleGuard();

  const state = lock?.lock_state ?? 0;

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Smart Lock</Text>

      <View style={[styles.stateCard, { backgroundColor: LOCK_COLORS[state] || '#333' }]}>
        <Text style={styles.stateLabel}>{LOCK_LABELS[state] || 'Unknown'}</Text>
        {lock && (
          <Text style={styles.stateSubtext}>
            Battery: {lock.battery}%
          </Text>
        )}
      </View>

      {lock && state >= 3 && (
        <View style={styles.gpsCard}>
          <Text style={styles.gpsLabel}>GPS Location:</Text>
          <Text style={styles.gpsValue}>
            {lock.gps_lat.toFixed(6)}, {lock.gps_lon.toFixed(6)}
          </Text>
        </View>
      )}

      <View style={styles.buttonRow}>
        <Button title="Arm Lock" onPress={armLock} color="#e74c3c"
          disabled={state !== 0} />
        <Button title="Disarm Lock" onPress={disarmLock} color="#2ecc71"
          disabled={state === 0} />
      </View>

      <Text style={styles.hint}>
        Arm when parking. Lock will auto-track via GPS + 4G LTE if moved.
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 20 },
  stateCard: { padding: 24, borderRadius: 12, alignItems: 'center', marginBottom: 15 },
  stateLabel: { fontSize: 28, fontWeight: 'bold', color: '#fff' },
  stateSubtext: { fontSize: 14, color: '#fff', opacity: 0.8, marginTop: 4 },
  gpsCard: { backgroundColor: '#1a1a1a', padding: 16, borderRadius: 10, marginBottom: 15 },
  gpsLabel: { fontSize: 12, color: '#7f8c8d' },
  gpsValue: { fontSize: 16, color: '#fff', marginTop: 4, fontFamily: 'monospace' },
  buttonRow: { flexDirection: 'row', justifyContent: 'space-around', marginBottom: 15 },
  hint: { fontSize: 12, color: '#7f8c8d', textAlign: 'center' },
});