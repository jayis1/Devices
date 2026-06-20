// HomeScreen — Stress score + quick stats

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import StressGauge from '../components/StressGauge';
import { getStress, AlertWebSocket, StressData } from '../api';

const USER_ID = 1;

export default function HomeScreen() {
  const [stress, setStress] = useState<StressData>({
    current: { stress: 0, burnout_risk: 0, recovery: 100 },
    trend: []
  });
  const [refreshing, setRefreshing] = useState(false);
  const [lastAlert, setLastAlert] = useState<string | null>(null);

  const fetchStress = useCallback(async () => {
    try {
      const data = await getStress(USER_ID);
      setStress(data);
    } catch (e) {
      console.error('Failed to fetch stress:', e);
    }
  }, []);

  useEffect(() => {
    fetchStress();
    const ws = new AlertWebSocket(USER_ID, (alert) => {
      setLastAlert(alert.message);
    });
    ws.connect();
    const interval = setInterval(fetchStress, 60000);
    return () => { ws.disconnect(); clearInterval(interval); };
  }, [fetchStress]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchStress();
    setRefreshing(false);
  }, [fetchStress]);

  const cur = stress.current;

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <Text style={styles.header}>🌿 CalmGrid</Text>

      {lastAlert && (
        <View style={styles.alertBanner}>
          <Text style={styles.alertText}>⚠️ {lastAlert}</Text>
        </View>
      )}

      {cur && (
        <StressGauge
          stress={cur.stress}
          burnoutRisk={cur.burnout_risk}
          recovery={cur.recovery}
        />
      )}

      <View style={styles.stats}>
        <View style={styles.statCard}>
          <Text style={styles.statTitle}>Status</Text>
          <Text style={[styles.statValue, {
            color: cur && cur.stress < 30 ? '#4CAF50' :
                   cur && cur.stress < 50 ? '#FFC107' :
                   cur && cur.stress < 70 ? '#FF9800' : '#F44336'
          }]}>
            {cur && cur.stress < 30 ? 'Calm' :
             cur && cur.stress < 50 ? 'Normal' :
             cur && cur.stress < 70 ? 'Elevated' : 'High Stress'}
          </Text>
        </View>
        <View style={styles.statCard}>
          <Text style={styles.statTitle}>14-Day Avg</Text>
          <Text style={styles.statValue}>
            {stress.trend.length > 0
              ? `${Math.round(stress.trend.reduce((a, t) => a + t.stress, 0) / stress.trend.length)}`
              : '—'}
          </Text>
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 28, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  alertBanner: { backgroundColor: '#FFF3E0', padding: 12, margin: 16, borderRadius: 8 },
  alertText: { color: '#E65100', fontSize: 14 },
  stats: { flexDirection: 'row', justifyContent: 'space-around', padding: 16 },
  statCard: { backgroundColor: 'white', padding: 16, borderRadius: 12, alignItems: 'center', flex: 1, marginHorizontal: 8 },
  statTitle: { fontSize: 12, color: '#666', marginBottom: 4 },
  statValue: { fontSize: 20, fontWeight: 'bold' },
});