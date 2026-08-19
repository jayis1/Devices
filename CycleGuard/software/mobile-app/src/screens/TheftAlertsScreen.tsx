import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { useCycleGuard } from '../services/CycleGuardContext';

export default function TheftAlertsScreen() {
  const { theftAlert, lock } = useCycleGuard();
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Theft Alerts</Text>
      {theftAlert ? (
        <View style={styles.alertCard}>
          <Text style={styles.alertTitle}>⚠ THEFT IN PROGRESS</Text>
          <Text style={styles.alertText}>
            GPS: {lock?.gps_lat.toFixed(6)}, {lock?.gps_lon.toFixed(6)}
          </Text>
          <Text style={styles.alertText}>120 dB siren activated</Text>
          <Text style={styles.alertText}>SMS sent to emergency contact</Text>
        </View>
      ) : (
        <Text style={styles.noAlerts}>No theft alerts. Your bike is safe.</Text>
      )}
    </View>
  );
}
const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 20 },
  alertCard: { backgroundColor: '#c0392b', padding: 20, borderRadius: 12 },
  alertTitle: { fontSize: 20, fontWeight: 'bold', color: '#fff', marginBottom: 10 },
  alertText: { fontSize: 14, color: '#fff', marginBottom: 4 },
  noAlerts: { fontSize: 16, color: '#7f8c8d', textAlign: 'center', marginTop: 40 },
});