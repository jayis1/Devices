// VitalsScreen — HR, HRV, EDA, temp trends

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { getVitals, Vitals } from '../api';

const USER_ID = 1;

export default function VitalsScreen() {
  const [vitals, setVitals] = useState<Vitals[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const data = await getVitals(USER_ID);
      setVitals(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  const latest = vitals.length > 0 ? vitals[vitals.length - 1] : null;

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <Text style={styles.header}>Vitals</Text>

      {latest && (
        <View style={styles.currentGrid}>
          <View style={styles.vitalCard}>
            <Text style={styles.vitalLabel}>Heart Rate</Text>
            <Text style={styles.vitalValue}>{latest.hr} bpm</Text>
          </View>
          <View style={styles.vitalCard}>
            <Text style={styles.vitalLabel}>HRV (RMSSD)</Text>
            <Text style={styles.vitalValue}>{latest.hrv_ms?.toFixed(1)} ms</Text>
          </View>
          <View style={styles.vitalCard}>
            <Text style={styles.vitalLabel}>EDA (SCL)</Text>
            <Text style={styles.vitalValue}>{latest.eda_scl?.toFixed(1)} µS</Text>
          </View>
          <View style={styles.vitalCard}>
            <Text style={styles.vitalLabel}>SCR Rate</Text>
            <Text style={styles.vitalValue}>{latest.eda_scr?.toFixed(1)}/min</Text>
          </View>
          <View style={styles.vitalCard}>
            <Text style={styles.vitalLabel}>Skin Temp</Text>
            <Text style={styles.vitalValue}>{latest.temp_c?.toFixed(1)}°C</Text>
          </View>
          <View style={styles.vitalCard}>
            <Text style={styles.vitalLabel}>Steps</Text>
            <Text style={styles.vitalValue}>{latest.steps}</Text>
          </View>
        </View>
      )}

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>HRV Trend (24h)</Text>
        {vitals.length > 0 && (
          <View style={styles.trendBar}>
            {vitals.slice(-48).map((v, i) => (
              <View key={i} style={[styles.trendBarSegment, {
                height: `${Math.min(v.hrv_ms / 100 * 100, 100)}%`,
                backgroundColor: v.hrv_ms > 40 ? '#4CAF50' : v.hrv_ms > 25 ? '#FFC107' : '#F44336'
              }]} />
            ))}
          </View>
        )}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  currentGrid: { flexDirection: 'row', flexWrap: 'wrap', padding: 8, justifyContent: 'space-between' },
  vitalCard: { backgroundColor: 'white', width: '48%', padding: 16, borderRadius: 12, marginBottom: 8, alignItems: 'center' },
  vitalLabel: { fontSize: 12, color: '#666', marginBottom: 4 },
  vitalValue: { fontSize: 20, fontWeight: 'bold', color: '#333' },
  section: { backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 12 },
  trendBar: { flexDirection: 'row', alignItems: 'flex-end', height: 100, gap: 2 },
  trendBarSegment: { flex: 1, borderRadius: 2 },
});