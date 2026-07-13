/**
 * LiveDashboardScreen — Real-time cardiovascular dashboard
 *
 * Shows: current HR, rhythm status, SpO₂, latest BP, AFib status
 *
 * License: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ActivityIndicator } from 'react-native';
import { useApi } from '../api/client';

export default function LiveDashboardScreen() {
  const api = useApi();
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const dashboard = await api.getDashboard();
        setData(dashboard);
      } catch (e) {
        console.error('Failed to fetch dashboard:', e);
      } finally {
        setLoading(false);
      }
    };
    fetchData();
    const interval = setInterval(fetchData, 5000);
    return () => clearInterval(interval);
  }, []);

  if (loading) return <ActivityIndicator size="large" color="#e74c3c" />;
  if (!data) return <Text style={styles.error}>Unable to connect to CardioSync</Text>;

  const rhythmStatus = data.afib_events_24h > 0 ? 'AFib Detected' : 'Normal Sinus';
  const rhythmColor = data.afib_events_24h > 0 ? '#e74c3c' : '#27ae60';

  return (
    <View style={styles.container}>
      <Text style={styles.title}>CardioSync Live</Text>

      <View style={styles.card}>
        <Text style={styles.cardLabel}>Heart Rate</Text>
        <Text style={styles.cardValue}>{data.heart_rate} bpm</Text>
      </View>

      <View style={[styles.card, { borderColor: rhythmColor }]}>
        <Text style={styles.cardLabel}>Rhythm Status</Text>
        <Text style={[styles.cardValue, { color: rhythmColor }]}>{rhythmStatus}</Text>
        {data.afib_events_24h > 0 && (
          <Text style={styles.subText}>{data.afib_events_24h} AFib events in 24h</Text>
        )}
      </View>

      <View style={styles.row}>
        <View style={styles.cardSmall}>
          <Text style={styles.cardLabel}>SpO₂</Text>
          <Text style={styles.cardValue}>{data.spo2}%</Text>
        </View>
        <View style={styles.cardSmall}>
          <Text style={styles.cardLabel}>Blood Pressure</Text>
          <Text style={styles.cardValue}>
            {data.blood_pressure
              ? `${data.blood_pressure.systolic}/${data.blood_pressure.diastolic}`
              : '—'}
          </Text>
        </View>
      </View>

      {data.hrv && (
        <View style={styles.card}>
          <Text style={styles.cardLabel}>HRV (RMSSD)</Text>
          <Text style={styles.cardValue}>{data.hrv.rmssd} ms</Text>
        </View>
      )}

      <Text style={styles.timestamp}>
        Updated: {new Date(data.timestamp).toLocaleTimeString()}
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#2c3e50', padding: 20 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 20 },
  card: {
    backgroundColor: '#34495e', borderRadius: 12, padding: 20,
    marginBottom: 12, borderWidth: 2, borderColor: '#34495e',
  },
  cardSmall: {
    backgroundColor: '#34495e', borderRadius: 12, padding: 15,
    flex: 1, marginHorizontal: 6, marginBottom: 12, borderWidth: 2,
    borderColor: '#34495e',
  },
  row: { flexDirection: 'row' },
  cardLabel: { fontSize: 14, color: '#95a5a6', marginBottom: 5 },
  cardValue: { fontSize: 32, fontWeight: 'bold', color: '#fff' },
  subText: { fontSize: 12, color: '#95a5a6', marginTop: 5 },
  timestamp: { fontSize: 12, color: '#7f8c8d', textAlign: 'center', marginTop: 10 },
  error: { color: '#e74c3c', fontSize: 16, textAlign: 'center', marginTop: 50 },
});