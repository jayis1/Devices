/**
 * SightSync Mobile App — Live Dashboard Screen
 *
 * Shows: Visual Fatigue Index, blink rate, viewing distance,
 * ambient light, minutes since break, dry-eye risk.
 *
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, RefreshControl, ScrollView } from 'react-native';
import { Card, ProgressBar, IconButton } from 'react-native-paper';
import api from '../api/client';

export default function LiveDashboardScreen() {
  const [data, setData] = useState({
    fatigue_score: 0,
    alert_level: 0,
    blink_rate: 0,
    viewing_distance_mm: 0,
    ambient_lux: 0,
    minutes_since_break: 0,
  });
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = async () => {
    try {
      const res = await api.getCurrentFatigue();
      setData(res.data);
    } catch (e) {
      console.error('Failed to fetch fatigue data:', e);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 10000); // refresh every 10s
    return () => clearInterval(interval);
  }, []);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  }, []);

  const fatigueColor = (score: number) => {
    if (score >= 85) return '#D32F2F';  // critical - red
    if (score >= 70) return '#F57C00';  // high - orange
    if (score >= 50) return '#FBC02D';  // moderate - yellow
    if (score >= 30) return '#7CB342';  // low - light green
    return '#43A047';                    // none - green
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Fatigue Index Card */}
      <Card style={styles.card}>
        <Card.Title title="Visual Fatigue Index" subtitle="Real-time eye strain score" />
        <Card.Content>
          <View style={styles.scoreRow}>
            <Text style={[styles.scoreValue, { color: fatigueColor(data.fatigue_score) }]}>
              {data.fatigue_score}
            </Text>
            <Text style={styles.scoreUnit}>/ 100</Text>
          </View>
          <ProgressBar
            progress={data.fatigue_score / 100}
            color={fatigueColor(data.fatigue_score)}
            style={styles.progressBar}
          />
          <Text style={styles.alertLabel}>
            Alert Level: {['None', 'Low', 'Moderate', 'High', 'Critical'][data.alert_level]}
          </Text>
        </Card.Content>
      </Card>

      {/* Metrics Grid */}
      <View style={styles.grid}>
        <Card style={styles.metricCard}>
          <Card.Content style={styles.metricContent}>
            <Text style={styles.metricLabel}>Blink Rate</Text>
            <Text style={styles.metricValue}>{data.blink_rate}</Text>
            <Text style={styles.metricUnit}>bpm</Text>
          </Card.Content>
        </Card>

        <Card style={styles.metricCard}>
          <Card.Content style={styles.metricContent}>
            <Text style={styles.metricLabel}>Distance</Text>
            <Text style={styles.metricValue}>{data.viewing_distance_mm}</Text>
            <Text style={styles.metricUnit}>mm</Text>
          </Card.Content>
        </Card>

        <Card style={styles.metricCard}>
          <Card.Content style={styles.metricContent}>
            <Text style={styles.metricLabel}>Ambient</Text>
            <Text style={styles.metricValue}>{data.ambient_lux}</Text>
            <Text style={styles.metricUnit}>lux</Text>
          </Card.Content>
        </Card>

        <Card style={styles.metricCard}>
          <Card.Content style={styles.metricContent}>
            <Text style={styles.metricLabel}>Since Break</Text>
            <Text style={styles.metricValue}>{data.minutes_since_break}</Text>
            <Text style={styles.metricUnit}>min</Text>
          </Card.Content>
        </Card>
      </View>

      {/* Recommendations */}
      <Card style={styles.card}>
        <Card.Title title="Recommendations" />
        <Card.Content>
          {data.fatigue_score >= 70 && (
            <Text style={styles.recommendation}>⚠️ High fatigue — take a 20-second break now</Text>
          )}
          {data.blink_rate < 8 && (
            <Text style={styles.recommendation}>👁️ Low blink rate — practice conscious blinking</Text>
          )}
          {data.viewing_distance_mm < 300 && data.viewing_distance_mm > 0 && (
            <Text style={styles.recommendation}>📏 Too close — move back to 50+ cm</Text>
          )}
          {data.ambient_lux < 300 && (
            <Text style={styles.recommendation}>💡 Insufficient lighting — increase ambient light</Text>
          )}
          {data.minutes_since_break >= 20 && (
            <Text style={styles.recommendation}>⏰ 20-20-20 break overdue!</Text>
          )}
          {data.fatigue_score < 30 && data.blink_rate >= 12 && (
            <Text style={styles.recommendation}>✅ Eye health looks good — keep it up!</Text>
          )}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 8, elevation: 2 },
  scoreRow: { flexDirection: 'row', alignItems: 'baseline', marginBottom: 8 },
  scoreValue: { fontSize: 64, fontWeight: 'bold' },
  scoreUnit: { fontSize: 20, color: '#888', marginLeft: 4 },
  progressBar: { height: 8, borderRadius: 4, marginVertical: 8 },
  alertLabel: { fontSize: 14, color: '#666', marginTop: 4 },
  grid: { flexDirection: 'row', flexWrap: 'wrap', paddingHorizontal: 4 },
  metricCard: { flex: 1, minWidth: '45%', margin: 4, elevation: 2 },
  metricContent: { alignItems: 'center', paddingVertical: 12 },
  metricLabel: { fontSize: 12, color: '#888' },
  metricValue: { fontSize: 32, fontWeight: 'bold', color: '#333' },
  metricUnit: { fontSize: 12, color: '#888' },
  recommendation: { fontSize: 14, marginVertical: 4, color: '#333' },
});