// VitalsScreen — HR, HRV, temp trends

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { getVitals } from '../api';

const PET_ID = 1;

export default function VitalsScreen() {
  const [vitals, setVitals] = useState<any>(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetchVitals = useCallback(async () => {
    try {
      const data = await getVitals(PET_ID);
      setVitals(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => {
    fetchVitals();
    const interval = setInterval(fetchVitals, 30000);
    return () => clearInterval(interval);
  }, [fetchVitals]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchVitals();
    setRefreshing(false);
  };

  const cur = vitals?.current || { hr: 0, hrv_ms: 0, temp_c: 0, activity: 0 };
  const base = vitals?.baseline || { hr: 0, hrv_ms: 0, established: false };
  const activityNames = ['Resting', 'Walking', 'Running', 'Sleeping', 'Scratching', 'Head Shake', 'Licking', 'Eating', 'Playing'];

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <Text style={styles.header}>📊 Vital Signs</Text>

      <View style={styles.vitalCard}>
        <Text style={styles.vitalIcon}>❤️</Text>
        <View style={styles.vitalInfo}>
          <Text style={styles.vitalLabel}>Heart Rate</Text>
          <Text style={styles.vitalValue}>{cur.hr} <Text style={styles.vitalUnit}>bpm</Text></Text>
          {base.established && (
            <Text style={styles.vitalBaseline}>Baseline: {base.hr.toFixed(0)} bpm</Text>
          )}
        </View>
        {base.established && cur.hr > base.hr * 1.15 && (
          <Text style={styles.vitalAlert}>⚠️</Text>
        )}
      </View>

      <View style={styles.vitalCard}>
        <Text style={styles.vitalIcon}>💓</Text>
        <View style={styles.vitalInfo}>
          <Text style={styles.vitalLabel}>Heart Rate Variability</Text>
          <Text style={styles.vitalValue}>{cur.hrv_ms.toFixed(1)} <Text style={styles.vitalUnit}>ms</Text></Text>
          {base.established && (
            <Text style={styles.vitalBaseline}>Baseline: {base.hrv_ms.toFixed(1)} ms</Text>
          )}
        </View>
        {base.established && cur.hrv_ms < base.hrv_ms * 0.8 && (
          <Text style={styles.vitalAlert}>⚠️</Text>
        )}
      </View>

      <View style={styles.vitalCard}>
        <Text style={styles.vitalIcon}>🌡️</Text>
        <View style={styles.vitalInfo}>
          <Text style={styles.vitalLabel}>Skin Temperature</Text>
          <Text style={styles.vitalValue}>{cur.temp_c.toFixed(1)}<Text style={styles.vitalUnit}>°C</Text></Text>
        </View>
      </View>

      <View style={styles.vitalCard}>
        <Text style={styles.vitalIcon}>🐾</Text>
        <View style={styles.vitalInfo}>
          <Text style={styles.vitalLabel}>Current Activity</Text>
          <Text style={styles.vitalValue}>{activityNames[cur.activity] || 'Unknown'}</Text>
        </View>
      </View>

      {!base.established && (
        <View style={styles.noticeCard}>
          <Text style={styles.noticeTitle}>📊 Learning Baseline</Text>
          <Text style={styles.noticeText}>
            PawSync is learning your pet's baseline vitals. This takes about 14 days.
            Wellness alerts will activate once the baseline is established.
          </Text>
        </View>
      )}

      <Text style={styles.sectionTitle}>24-Hour HR/HRV Trend</Text>
      <View style={styles.chartPlaceholder}>
        <Text style={styles.chartText}>📈 Trend chart appears here</Text>
        <Text style={styles.chartSubtext}>
          {vitals?.trend?.length || 0} data points
        </Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 22, fontWeight: 'bold', textAlign: 'center', marginVertical: 16 },
  vitalCard: { flexDirection: 'row', alignItems: 'center', backgroundColor: 'white', marginHorizontal: 16, marginBottom: 8, padding: 16, borderRadius: 12 },
  vitalIcon: { fontSize: 28, marginRight: 16 },
  vitalInfo: { flex: 1 },
  vitalLabel: { fontSize: 12, color: '#666' },
  vitalValue: { fontSize: 24, fontWeight: 'bold', color: '#333' },
  vitalUnit: { fontSize: 14, color: '#999' },
  vitalBaseline: { fontSize: 11, color: '#999', marginTop: 2 },
  vitalAlert: { fontSize: 24 },
  noticeCard: { backgroundColor: '#E3F2FD', margin: 16, padding: 16, borderRadius: 12 },
  noticeTitle: { fontSize: 14, fontWeight: '600', color: '#1565C0', marginBottom: 4 },
  noticeText: { fontSize: 12, color: '#666', lineHeight: 18 },
  sectionTitle: { fontSize: 16, fontWeight: '600', marginHorizontal: 16, marginVertical: 8 },
  chartPlaceholder: { backgroundColor: 'white', marginHorizontal: 16, marginBottom: 16, padding: 40, borderRadius: 12, alignItems: 'center' },
  chartText: { fontSize: 16, color: '#999' },
  chartSubtext: { fontSize: 12, color: '#ccc', marginTop: 4 },
});