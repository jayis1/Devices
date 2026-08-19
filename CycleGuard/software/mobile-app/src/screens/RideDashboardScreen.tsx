import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { useCycleGuard } from '../services/CycleGuardContext';

const BLINDSPOT_LABELS: Record<number, string> = {
  0: 'Clear', 1: 'Bicycle', 2: 'Motorcycle', 3: 'Car',
  4: 'Truck', 5: 'Bus', 6: 'Pedestrian', 7: 'Obstacle',
};

export default function RideDashboardScreen() {
  const { ride, collisionRisk, blindspotClass, crashAlert, connected } = useCycleGuard();

  return (
    <View style={styles.container}>
      <Text style={styles.title}>CycleGuard</Text>
      <Text style={styles.connection}>
        {connected ? '● Connected' : '○ Disconnected'}
      </Text>

      {/* Speed (primary metric) */}
      <View style={styles.speedCard}>
        <Text style={styles.speedValue}>
          {(ride?.speed_kmh || 0).toFixed(1)}
        </Text>
        <Text style={styles.speedUnit}>km/h</Text>
      </View>

      {/* Secondary metrics */}
      <View style={styles.metricsRow}>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Cadence</Text>
          <Text style={styles.metricValue}>{ride?.cadence_rpm || 0} rpm</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Tire PSI</Text>
          <Text style={styles.metricValue}>
            {(ride?.tire_pressure || 0).toFixed(0)}
          </Text>
        </View>
      </View>

      {/* Blind spot warning */}
      <View style={[styles.warningCard,
        blindspotClass > 0 ? styles.warningActive : null]}>
        <Text style={styles.warningLabel}>Rear</Text>
        <Text style={styles.warningValue}>
          {BLINDSPOT_LABELS[blindspotClass] || 'Clear'}
        </Text>
      </View>

      {/* Collision risk */}
      <View style={[styles.riskCard,
        collisionRisk > 0.6 ? styles.riskHigh : collisionRisk > 0.3 ? styles.riskMod : null]}>
        <Text style={styles.riskLabel}>Collision Risk</Text>
        <Text style={styles.riskValue}>{(collisionRisk * 100).toFixed(0)}%</Text>
      </View>

      {/* Crash alert */}
      {crashAlert && (
        <View style={styles.crashAlert}>
          <Text style={styles.crashText}>⚠ CRASH DETECTED</Text>
          <Text style={styles.crashSubtext}>Emergency services dispatched</Text>
        </View>
      )}

      {/* Battery */}
      <Text style={styles.batteryText}>Hub battery: {ride?.battery || 0}%</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 28, fontWeight: 'bold', color: '#2ecc71', marginBottom: 2 },
  connection: { fontSize: 12, color: '#7f8c8d', marginBottom: 15 },
  speedCard: { alignItems: 'center', padding: 20, marginBottom: 15,
    backgroundColor: '#1a1a1a', borderRadius: 12 },
  speedValue: { fontSize: 64, fontWeight: 'bold', color: '#fff' },
  speedUnit: { fontSize: 18, color: '#7f8c8d' },
  metricsRow: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 10 },
  metricCard: { flex: 1, backgroundColor: '#1a1a1a', padding: 14, borderRadius: 10,
    marginHorizontal: 4, alignItems: 'center' },
  metricLabel: { fontSize: 12, color: '#7f8c8d', marginBottom: 4 },
  metricValue: { fontSize: 20, fontWeight: 'bold', color: '#fff' },
  warningCard: { backgroundColor: '#1a1a1a', padding: 14, borderRadius: 10,
    marginBottom: 10, flexDirection: 'row', justifyContent: 'space-between' },
  warningActive: { backgroundColor: '#e74c3c' },
  warningLabel: { fontSize: 14, color: '#7f8c8d' },
  warningValue: { fontSize: 18, fontWeight: 'bold', color: '#fff' },
  riskCard: { backgroundColor: '#1a1a1a', padding: 14, borderRadius: 10,
    marginBottom: 10, flexDirection: 'row', justifyContent: 'space-between' },
  riskHigh: { backgroundColor: '#e74c3c' },
  riskMod: { backgroundColor: '#f39c12' },
  riskLabel: { fontSize: 14, color: '#7f8c8d' },
  riskValue: { fontSize: 18, fontWeight: 'bold', color: '#fff' },
  crashAlert: { backgroundColor: '#c0392b', padding: 16, borderRadius: 10,
    marginTop: 5, alignItems: 'center' },
  crashText: { fontSize: 20, fontWeight: 'bold', color: '#fff' },
  crashSubtext: { fontSize: 14, color: '#fff', opacity: 0.9, marginTop: 4 },
  batteryText: { fontSize: 12, color: '#7f8c8d', textAlign: 'center', marginTop: 10 },
});