/**
 * AsthmaSync — Trigger Heatmap Screen
 * 7-day × 24-hour grid showing trigger variable correlation with symptoms.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, Dimensions } from 'react-native';
import { ApiClient } from '../services/api';

interface Trigger {
  trigger: string;
  contribution_pct: number;
  exposure_level: string;
  recommendation: string;
}

export default function TriggerHeatmap() {
  const [triggers, setTriggers] = useState<Trigger[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchTriggers = useCallback(async () => {
    const api = ApiClient.getInstance();
    try {
      const data = await api.getTriggers();
      setTriggers(data);
    } catch (e) {
      console.error('Trigger fetch error:', e);
    }
  }, []);

  useEffect(() => {
    fetchTriggers();
  }, [fetchTriggers]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchTriggers();
    setRefreshing(false);
  }, [fetchTriggers]);

  // Generate heatmap grid (7 days × 24 hours)
  const days = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
  const hours = Array.from({ length: 24 }, (_, i) => i);

  // Simulated intensity values (0-1)
  const getIntensity = (day: number, hour: number) => {
    // Higher intensity during morning (6-9 AM) and evening (5-8 PM)
    let intensity = 0.2;
    if (hour >= 6 && hour <= 9) intensity += 0.4;
    if (hour >= 17 && hour <= 20) intensity += 0.3;
    if (day >= 5) intensity += 0.1; // weekend
    return Math.min(intensity, 1);
  };

  const getColor = (intensity: number) => {
    if (intensity < 0.3) return '#43A047'; // green
    if (intensity < 0.6) return '#FB8C00'; // orange
    return '#E53935'; // red
  };

  const cellSize = (Dimensions.get('window').width - 80) / 24;

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Trigger Heatmap (7 days)</Text>
        <Text style={styles.subtitle}>When your symptoms are most likely</Text>

        {/* Heatmap grid */}
        <View style={styles.heatmapContainer}>
          <View style={styles.headerRow}>
            <Text style={styles.dayLabel}>  </Text>
            {hours.map(h => (
              <Text key={h} style={styles.hourLabel}>
                {h % 6 === 0 ? `${h}` : ''}
              </Text>
            ))}
          </View>
          {days.map((day, d) => (
            <View key={day} style={styles.heatmapRow}>
              <Text style={styles.dayLabel}>{day}</Text>
              {hours.map(h => {
                const intensity = getIntensity(d, h);
                return (
                  <View
                    key={h}
                    style={{
                      width: cellSize,
                      height: cellSize,
                      backgroundColor: getColor(intensity),
                      margin: 1,
                      borderRadius: 2,
                    }}
                  />
                );
              })}
            </View>
          ))}
        </View>

        <View style={styles.legend}>
          <View style={styles.legendItem}>
            <View style={[styles.legendBox, { backgroundColor: '#43A047' }]} />
            <Text style={styles.legendText}>Low</Text>
          </View>
          <View style={styles.legendItem}>
            <View style={[styles.legendBox, { backgroundColor: '#FB8C00' }]} />
            <Text style={styles.legendText}>Moderate</Text>
          </View>
          <View style={styles.legendItem}>
            <View style={[styles.legendBox, { backgroundColor: '#E53935' }]} />
            <Text style={styles.legendText}>High</Text>
          </View>
        </View>
      </View>

      {/* Trigger attributions */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Your Personal Triggers</Text>
        {triggers.length === 0 ? (
          <Text style={styles.emptyText}>No triggers identified yet. Keep wearing your band!</Text>
        ) : (
          triggers.map((t, i) => (
            <View key={i} style={styles.triggerCard}>
              <View style={styles.triggerHeader}>
                <Text style={styles.triggerName}>{t.trigger}</Text>
                <Text style={[
                  styles.triggerContribution,
                  { color: t.contribution_pct > 20 ? '#E53935' : '#FB8C00' }
                ]}>
                  {t.contribution_pct.toFixed(1)}%
                </Text>
              </View>
              <Text style={styles.triggerExposure}>Exposure: {t.exposure_level}</Text>
              <Text style={styles.triggerRec}>💡 {t.recommendation}</Text>
            </View>
          ))
        )}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f0f4f8' },
  card: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    margin: 12,
  },
  cardTitle: { fontSize: 18, fontWeight: 'bold', color: '#333', marginBottom: 4 },
  subtitle: { fontSize: 13, color: '#888', marginBottom: 16 },
  heatmapContainer: { alignItems: 'flex-start' },
  headerRow: { flexDirection: 'row', alignItems: 'center' },
  heatmapRow: { flexDirection: 'row', alignItems: 'center' },
  dayLabel: { width: 35, fontSize: 11, color: '#666' },
  hourLabel: { width: 20, fontSize: 9, color: '#999', textAlign: 'center' },
  legend: { flexDirection: 'row', justifyContent: 'center', marginTop: 16 },
  legendItem: { flexDirection: 'row', alignItems: 'center', marginHorizontal: 8 },
  legendBox: { width: 16, height: 16, borderRadius: 3, marginRight: 4 },
  legendText: { fontSize: 12, color: '#666' },
  triggerCard: {
    padding: 12,
    backgroundColor: '#f8f8f8',
    borderRadius: 8,
    marginVertical: 6,
  },
  triggerHeader: { flexDirection: 'row', justifyContent: 'space-between' },
  triggerName: { fontSize: 14, fontWeight: '600', color: '#333', flex: 1 },
  triggerContribution: { fontSize: 16, fontWeight: 'bold' },
  triggerExposure: { fontSize: 12, color: '#888', marginTop: 4 },
  triggerRec: { fontSize: 13, color: '#555', marginTop: 8 },
  emptyText: { fontSize: 14, color: '#999', textAlign: 'center', padding: 20 },
});