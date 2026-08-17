import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { useTremorSync } from '../services/TremorSyncContext';

const STATE_COLORS: Record<number, string> = {
  0: '#e74c3c',  // OFF - red
  1: '#2ecc71',  // ON - green
  2: '#f39c12',  // TRANSITION - yellow
};
const STATE_LABELS: Record<number, string> = {
  0: 'OFF', 1: 'ON', 2: 'TRANSITION',
};
const TREMOR_LABELS: Record<number, string> = {
  0: 'None', 1: 'Resting', 2: 'Postural', 3: 'Action',
};

export default function DashboardScreen() {
  const { tremor, onoffState, nextDoseMin, fogActive, connected } = useTremorSync();

  return (
    <View style={styles.container}>
      <Text style={styles.title}>TremorSync</Text>
      <Text style={styles.connection}>
        {connected ? 'Connected' : 'Disconnected'}
      </Text>

      {/* ON/OFF State */}
      <View style={[styles.stateCard,
        { backgroundColor: STATE_COLORS[onoffState] || '#999' }]}>
        <Text style={styles.stateLabel}>
          State: {STATE_LABELS[onoffState] || 'Unknown'}
        </Text>
        <Text style={styles.stateSubtext}>
          {onoffState === 1 ? 'Motor function good' :
           onoffState === 0 ? 'Medication may be needed' :
           'Transitioning'}
        </Text>
      </View>

      {/* Tremor Score */}
      <View style={styles.metricCard}>
        <Text style={styles.metricLabel}>Tremor</Text>
        <Text style={styles.metricValue}>
          {TREMOR_LABELS[tremor?.tremor_class || 0]}
        </Text>
        <Text style={styles.metricSubtext}>
          Amplitude: {(tremor?.tremor_amplitude || 0).toFixed(3)} m/s²
        </Text>
      </View>

      {/* Bradykinesia */}
      <View style={styles.metricCard}>
        <Text style={styles.metricLabel}>Bradykinesia</Text>
        <Text style={styles.metricValue}>
          {(tremor?.bradykinesia || 0).toFixed(1)} / 100
        </Text>
      </View>

      {/* Next Dose */}
      <View style={styles.metricCard}>
        <Text style={styles.metricLabel}>Next Dose</Text>
        <Text style={styles.metricValue}>
          {nextDoseMin > 0 ? `${nextDoseMin} min` : 'Due now'}
        </Text>
      </View>

      {/* FOG Warning */}
      {fogActive && (
        <View style={styles.fogAlert}>
          <Text style={styles.fogText}>Freezing of Gait Detected</Text>
          <Text style={styles.fogSubtext}>Cueing activated</Text>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 28, fontWeight: 'bold', marginBottom: 5, color: '#2c3e50' },
  connection: { fontSize: 12, color: '#7f8c8d', marginBottom: 15 },
  stateCard: { padding: 20, borderRadius: 12, marginBottom: 12, alignItems: 'center' },
  stateLabel: { fontSize: 24, fontWeight: 'bold', color: '#fff' },
  stateSubtext: { fontSize: 14, color: '#fff', opacity: 0.9, marginTop: 4 },
  metricCard: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  metricLabel: { fontSize: 14, color: '#7f8c8d', marginBottom: 4 },
  metricValue: { fontSize: 22, fontWeight: 'bold', color: '#2c3e50' },
  metricSubtext: { fontSize: 12, color: '#95a5a6', marginTop: 2 },
  fogAlert: { backgroundColor: '#e74c3c', padding: 16, borderRadius: 10, marginTop: 5 },
  fogText: { fontSize: 18, fontWeight: 'bold', color: '#fff', textAlign: 'center' },
  fogSubtext: { fontSize: 14, color: '#fff', textAlign: 'center', opacity: 0.9 },
});