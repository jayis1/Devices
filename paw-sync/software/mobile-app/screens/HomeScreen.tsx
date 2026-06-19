// HomeScreen — Wellness score + quick stats

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import WellnessGauge from '../components/WellnessGauge';
import { getWellness, AlertWebSocket, WellnessData } from '../api';

const PET_ID = 1; // TODO: from pet profile

export default function HomeScreen() {
  const [wellness, setWellness] = useState<WellnessData>({
    wellness: 100, illness_risk: 0, anxiety_level: 0, trend: []
  });
  const [refreshing, setRefreshing] = useState(false);
  const [lastAlert, setLastAlert] = useState<string | null>(null);

  const fetchWellness = useCallback(async () => {
    try {
      const data = await getWellness(PET_ID);
      setWellness(data);
    } catch (e) {
      console.error('Failed to fetch wellness:', e);
    }
  }, []);

  useEffect(() => {
    fetchWellness();
    // WebSocket for real-time alerts
    const ws = new AlertWebSocket(PET_ID, (alert) => {
      setLastAlert(alert.message);
    });
    ws.connect();
    const interval = setInterval(fetchWellness, 60000); // refresh every 60s
    return () => { ws.disconnect(); clearInterval(interval); };
  }, [fetchWellness]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchWellness();
    setRefreshing(false);
  }, [fetchWellness]);

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <Text style={styles.header}>🐾 {wellness.trend.length > 0 ? 'Buddy' : 'Your Pet'}</Text>

      {lastAlert && (
        <View style={styles.alertBanner}>
          <Text style={styles.alertText}>⚠️ {lastAlert}</Text>
        </View>
      )}

      <WellnessGauge
        wellness={wellness.wellness}
        illnessRisk={wellness.illness_risk}
        anxietyLevel={wellness.anxiety_level}
      />

      <View style={styles.stats}>
        <View style={styles.statCard}>
          <Text style={styles.statTitle}>7-Day Trend</Text>
          <Text style={styles.statValue}>
            {wellness.trend.length > 0
              ? `${Math.round(wellness.trend.reduce((a, t) => a + t.wellness, 0) / wellness.trend.length)} avg`
              : '—'}
          </Text>
        </View>
        <View style={styles.statCard}>
          <Text style={styles.statTitle}>Status</Text>
          <Text style={[styles.statValue, {
            color: wellness.wellness >= 70 ? '#4CAF50' : wellness.wellness >= 50 ? '#FF9800' : '#F44336'
          }]}>
            {wellness.wellness >= 70 ? 'Healthy' : wellness.wellness >= 50 ? 'Monitor' : 'Vet Visit'}
          </Text>
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginVertical: 20 },
  alertBanner: { backgroundColor: '#F44336', padding: 12, margin: 16, borderRadius: 8 },
  alertText: { color: 'white', fontSize: 14, fontWeight: '600' },
  stats: { flexDirection: 'row', justifyContent: 'space-around', padding: 16 },
  statCard: { backgroundColor: 'white', padding: 16, borderRadius: 12, width: '45%', alignItems: 'center' },
  statTitle: { fontSize: 12, color: '#666', marginBottom: 4 },
  statValue: { fontSize: 20, fontWeight: 'bold' },
});