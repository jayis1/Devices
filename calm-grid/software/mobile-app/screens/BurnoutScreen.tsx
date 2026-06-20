// BurnoutScreen — Burnout risk forecast + contributing factors

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { getBurnout, BurnoutData } from '../api';

const USER_ID = 1;

export default function BurnoutScreen() {
  const [data, setData] = useState<BurnoutData>({ risk: 0, current_stress: 0, avg_stress_30d: 0, trend: [] });
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const d = await getBurnout(USER_ID);
      setData(d);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  const riskColor = data.risk < 30 ? '#4CAF50' : data.risk < 50 ? '#FFC107' : data.risk < 70 ? '#FF9800' : '#F44336';

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <Text style={styles.header}>Burnout Forecast</Text>
      <Text style={styles.subtitle}>14-day risk based on 30-day physiological trends</Text>

      <View style={styles.riskCard}>
        <Text style={[styles.riskValue, { color: riskColor }]}>{data.risk}%</Text>
        <Text style={styles.riskLabel}>Burnout Risk</Text>
        <View style={styles.riskBar}>
          <View style={[styles.riskFill, { width: `${data.risk}%`, backgroundColor: riskColor }]} />
        </View>
        <Text style={styles.riskStatus}>
          {data.risk < 30 ? 'Low risk — well regulated' :
           data.risk < 50 ? 'Moderate — maintain self-care' :
           data.risk < 70 ? 'Elevated — increase recovery time' :
           'High risk — consider professional support'}
        </Text>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>30-Day Trend</Text>
        <Text style={styles.statRow}>Current stress: <Text style={styles.statVal}>{data.current_stress}/100</Text></Text>
        <Text style={styles.statRow}>30-day average: <Text style={styles.statVal}>{Math.round(data.avg_stress_30d)}/100</Text></Text>
      </View>

      {data.trend.length > 0 && (
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>Burnout Risk Trend</Text>
          <View style={styles.trendBar}>
            {data.trend.slice(-30).map((t, i) => (
              <View key={i} style={[styles.trendBarSegment, {
                height: `${t.burnout}%`,
                backgroundColor: t.burnout < 30 ? '#4CAF50' : t.burnout < 50 ? '#FFC107' : t.burnout < 70 ? '#FF9800' : '#F44336'
              }]} />
            ))}
          </View>
        </View>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  subtitle: { fontSize: 14, color: '#666', textAlign: 'center', marginBottom: 16 },
  riskCard: { backgroundColor: 'white', margin: 16, padding: 24, borderRadius: 16, alignItems: 'center' },
  riskValue: { fontSize: 56, fontWeight: 'bold' },
  riskLabel: { fontSize: 16, color: '#666', marginBottom: 16 },
  riskBar: { width: '100%', height: 10, backgroundColor: '#eee', borderRadius: 5, marginBottom: 12 },
  riskFill: { height: 10, borderRadius: 5 },
  riskStatus: { fontSize: 14, color: '#333', textAlign: 'center' },
  section: { backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 12 },
  statRow: { fontSize: 14, color: '#333', marginBottom: 8 },
  statVal: { fontWeight: 'bold' },
  trendBar: { flexDirection: 'row', alignItems: 'flex-end', height: 80, gap: 2 },
  trendBarSegment: { flex: 1, borderRadius: 2 },
});