// SkinSync UVDashboardScreen — real-time UV exposure tracker

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl, ProgressBar } from 'react-native';
import { getUVHistory, UVEvent, UV_STATUS_NAMES, UV_STATUS_COLORS } from '../api';

export default function UVDashboardScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [events, setEvents] = useState<UVEvent[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const data = await getUVHistory(userId, 24);
      setEvents(data);
    } catch (e) {
      console.error('Failed to load UV data:', e);
    }
    setRefreshing(false);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  const latest = events[events.length - 1];
  const medFraction = latest?.med_frac ?? 0;
  const uvStatus = latest?.uv_status ?? 0;
  const todayUVA = events.reduce((sum, e) => sum + e.uva, 0);
  const todayUVB = events.reduce((sum, e) => sum + e.uvb, 0);

  return (
    <ScrollView style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} />}>
      <Text style={styles.title}>☀️ UV Exposure</Text>

      {/* MED Fraction Gauge */}
      <View style={[styles.medCard, { borderColor: UV_STATUS_COLORS[uvStatus] }]}>
        <Text style={styles.medLabel}>Today's Sun Burn Risk</Text>
        <Text style={[styles.medValue, { color: UV_STATUS_COLORS[uvStatus] }]}>
          {medFraction}%
        </Text>
        <Text style={styles.medSubtext}>{UV_STATUS_NAMES[uvStatus]}</Text>
        <ProgressBar
          progress={medFraction / 100}
          color={UV_STATUS_COLORS[uvStatus]}
          style={styles.progressBar}
        />
        {medFraction >= 70 && (
          <Text style={styles.burnWarning}>
            ⚠ Seek shade or reapply sunscreen!
          </Text>
        )}
      </View>

      {/* UV Dose Summary */}
      <View style={styles.doseRow}>
        <View style={styles.doseCard}>
          <Text style={styles.doseValue}>{todayUVA.toFixed(0)}</Text>
          <Text style={styles.doseLabel}>UVA (J/m²)</Text>
          <Text style={styles.doseNote}>Aging rays</Text>
        </View>
        <View style={styles.doseCard}>
          <Text style={styles.doseValue}>{todayUVB.toFixed(0)}</Text>
          <Text style={styles.doseLabel}>UVB (J/m²)</Text>
          <Text style={styles.doseNote}>Burning rays</Text>
        </View>
        <View style={styles.doseCard}>
          <Text style={styles.doseValue}>{latest?.uv_idx?.toFixed(1) ?? '0.0'}</Text>
          <Text style={styles.doseLabel}>UV Index</Text>
          <Text style={styles.doseNote}>Current</Text>
        </View>
      </View>

      {/* 24h UV Exposure Chart (simplified) */}
      <Text style={styles.sectionTitle}>24-Hour UV Exposure</Text>
      <View style={styles.chartContainer}>
        {events.slice(-48).map((e, i) => (
          <View
            key={i}
            style={[styles.chartBar, {
              height: Math.max(2, e.uv_idx * 8),
              backgroundColor: UV_STATUS_COLORS[e.uv_status] || '#4CAF50'
            }]}
          />
        ))}
      </View>

      <Text style={styles.footer}>
        UV patch: {latest ? `Active • ${latest.temp_c.toFixed(1)}°C skin temp` : 'No data'}
      </Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#E91E63' },
  medCard: { margin: 16, padding: 24, borderRadius: 16, backgroundColor: '#fff',
    borderWidth: 2, alignItems: 'center' },
  medLabel: { fontSize: 16, color: '#666' },
  medValue: { fontSize: 56, fontWeight: 'bold', marginVertical: 8 },
  medSubtext: { fontSize: 18, fontWeight: '600' },
  progressBar: { height: 8, width: '100%', borderRadius: 4, marginTop: 12 },
  burnWarning: { color: '#F44336', fontSize: 14, fontWeight: '600', marginTop: 12 },
  doseRow: { flexDirection: 'row', paddingHorizontal: 16, marginBottom: 16 },
  doseCard: { flex: 1, borderRadius: 12, padding: 16, marginHorizontal: 4,
    backgroundColor: '#fff', alignItems: 'center' },
  doseValue: { fontSize: 24, fontWeight: 'bold', color: '#333' },
  doseLabel: { fontSize: 12, color: '#888', marginTop: 4 },
  doseNote: { fontSize: 10, color: '#aaa' },
  sectionTitle: { fontSize: 18, fontWeight: 'bold', paddingHorizontal: 16, marginBottom: 8 },
  chartContainer: { flexDirection: 'row', alignItems: 'flex-end', height: 120,
    paddingHorizontal: 16, marginBottom: 16 },
  chartBar: { flex: 1, marginHorizontal: 1, borderRadius: 2 },
  footer: { fontSize: 12, color: '#999', textAlign: 'center', marginBottom: 20 },
});