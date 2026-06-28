/**
 * HeatmapScreen — Floor plan pest activity heatmap
 */
import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet, Dimensions } from 'react-native';
import { fetchHeatmap } from '../api/client';

const { width } = Dimensions.get('window');

export default function HeatmapScreen() {
  const [heatmap, setHeatmap] = useState<any>(null);

  useEffect(() => {
    fetchHeatmap().then(setHeatmap).catch(() => {});
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Pest Activity Heatmap</Text>

      {/* Zone activity bars */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Activity by Zone</Text>
        {heatmap?.zones?.map((zone: any, i: number) => (
          <View key={i} style={styles.zoneRow}>
            <Text style={styles.zoneName}>{zone.name}</Text>
            <View style={styles.barBg}>
              <View style={[styles.bar, { width: `${Math.min(zone.count * 5, 100)}%`, backgroundColor: getColor(zone.count) }]} />
            </View>
            <Text style={styles.zoneCount}>{zone.count}</Text>
          </View>
        ))}
      </View>

      {/* 24h heatmap grid */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Hourly Activity</Text>
        <View style={styles.hourGrid}>
          {heatmap?.hours?.map((count: number, hour: number) => (
            <View
              key={hour}
              style={[styles.hourCell, { backgroundColor: getColor(count) }]}
            >
              <Text style={styles.hourText}>{hour}</Text>
              <Text style={styles.countText}>{count}</Text>
            </View>
          ))}
        </View>
        <Text style={styles.legend}>Peak hour: {heatmap?.peakHour}:00 ({heatmap?.pattern})</Text>
      </View>

      {/* Trap placement recommendations */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Recommended Trap Placements</Text>
        {heatmap?.recommendations?.map((rec: any, i: number) => (
          <View key={i} style={styles.recRow}>
            <Text style={styles.recIcon}>📍</Text>
            <Text style={styles.recText}>{rec}</Text>
          </View>
        ))}
      </View>
    </ScrollView>
  );
}

function getColor(count: number): string {
  if (count === 0) return '#16213e';
  if (count < 2) return '#0f3460';
  if (count < 5) return '#1a5276';
  if (count < 10) return '#d35400';
  if (count < 20) return '#e67e22';
  return '#e74c3c';
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  title: { color: '#fff', fontSize: 22, fontWeight: 'bold', padding: 15 },
  card: { backgroundColor: '#16213e', margin: 10, padding: 15, borderRadius: 12 },
  cardTitle: { color: '#e0e0e0', fontSize: 16, fontWeight: 'bold', marginBottom: 10 },
  zoneRow: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
  zoneName: { color: '#fff', fontSize: 14, width: 80 },
  barBg: { flex: 1, height: 20, backgroundColor: '#0f3460', borderRadius: 10, marginHorizontal: 10 },
  bar: { height: 20, borderRadius: 10 },
  zoneCount: { color: '#95a5a6', fontSize: 14, width: 30, textAlign: 'right' },
  hourGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  hourCell: { width: (width - 60) / 6, height: 50, borderRadius: 6, margin: 3, justifyContent: 'center', alignItems: 'center' },
  hourText: { color: '#bdc3c7', fontSize: 10 },
  countText: { color: '#fff', fontSize: 14, fontWeight: 'bold' },
  legend: { color: '#95a5a6', fontSize: 12, marginTop: 10, textAlign: 'center' },
  recRow: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
  recIcon: { fontSize: 16, marginRight: 8 },
  recText: { color: '#bdc3c7', fontSize: 13, flex: 1 },
});