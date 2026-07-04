/**
 * GlucoSync — Analytics Screen
 * Time-in-range, AGP, insulin sensitivity.
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity } from 'react-native';
import { GlucoSyncAPI, TIRResult } from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function AnalyticsScreen() {
  const { userId } = useAuth();
  const [tir, setTir] = useState<TIRResult | null>(null);
  const [days, setDays] = useState(14);
  const [sensitivity, setSensitivity] = useState<any>(null);

  const fetchAnalytics = useCallback(async () => {
    if (!userId) return;
    try {
      const tirData = await GlucoSyncAPI.getTimeInRange(userId, days);
      setTir(tirData);
      const sensData = await GlucoSyncAPI.getInsulinSensitivity(userId);
      setSensitivity(sensData);
    } catch (e) { console.error(e); }
  }, [userId, days]);

  useEffect(() => { fetchAnalytics(); }, [fetchAnalytics]);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Analytics</Text>

      <View style={styles.periodRow}>
        {[7, 14, 30, 90].map(d => (
          <TouchableOpacity
            key={d}
            style={[styles.periodButton, days === d && styles.periodButtonActive]}
            onPress={() => setDays(d)}
          >
            <Text style={days === d ? styles.periodTextActive : styles.periodText}>{d}d</Text>
          </TouchableOpacity>
        ))}
      </View>

      {tir && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Time in Range</Text>
          <View style={styles.tirBar}>
            <View style={[styles.tirSegment, { flex: tir.below_pct, backgroundColor: '#DC2626' }]} />
            <View style={[styles.tirSegment, { flex: tir.tir_pct, backgroundColor: '#10B981' }]} />
            <View style={[styles.tirSegment, { flex: tir.above_pct, backgroundColor: '#F59E0B' }]} />
          </View>
          <View style={styles.tirLegend}>
            <Text style={styles.legendItem}>↓ {tir.below_pct}% (&lt;70)</Text>
            <Text style={styles.legendItem}>✓ {tir.tir_pct}% (70-180)</Text>
            <Text style={styles.legendItem}>↑ {tir.above_pct}% (&gt;180)</Text>
          </View>
          <View style={styles.statsGrid}>
            <View style={styles.statBox}>
              <Text style={styles.statLabel}>Avg Glucose</Text>
              <Text style={styles.statValue}>{tir.avg_glucose} mg/dL</Text>
            </View>
            <View style={styles.statBox}>
              <Text style={styles.statLabel}>GMI</Text>
              <Text style={styles.statValue}>{tir.gmi}%</Text>
            </View>
          </View>
        </View>
      )}

      {sensitivity && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Insulin Sensitivity</Text>
          <Text style={styles.sensitivityText}>
            I:C Ratio: <Text style={styles.sensitivityValue}>{sensitivity.ic_ratio} g/U</Text>
          </Text>
          <Text style={styles.sensitivityText}>
            ISF: <Text style={styles.sensitivityValue}>{sensitivity.isf} mg/dL/U</Text>
          </Text>
          <Text style={styles.sensitivityText}>
            Avg TDD: <Text style={styles.sensitivityValue}>{sensitivity.tdd_avg} U/day</Text>
          </Text>
          <Text style={styles.methodText}>
            Method: {sensitivity.method || (sensitivity.personalized ? 'personalized' : 'priors')}
          </Text>
        </View>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0F172A', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#F1F5F9', marginBottom: 16 },
  periodRow: { flexDirection: 'row', gap: 8, marginBottom: 16 },
  periodButton: { flex: 1, padding: 10, borderRadius: 8, backgroundColor: '#1E293B', alignItems: 'center' },
  periodButtonActive: { backgroundColor: '#2563EB' },
  periodText: { color: '#94A3B8', fontSize: 14 },
  periodTextActive: { color: '#FFFFFF', fontSize: 14, fontWeight: '600' },
  card: { backgroundColor: '#1E293B', borderRadius: 12, padding: 16, marginBottom: 16 },
  cardTitle: { fontSize: 18, fontWeight: 'bold', color: '#F1F5F9', marginBottom: 12 },
  tirBar: { flexDirection: 'row', height: 24, borderRadius: 6, overflow: 'hidden', marginBottom: 8 },
  tirSegment: { height: '100%' },
  tirLegend: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 12 },
  legendItem: { fontSize: 12, color: '#CBD5E1' },
  statsGrid: { flexDirection: 'row', gap: 12 },
  statBox: { flex: 1, backgroundColor: '#0F172A', borderRadius: 8, padding: 12, alignItems: 'center' },
  statLabel: { fontSize: 12, color: '#94A3B8', marginBottom: 4 },
  statValue: { fontSize: 18, fontWeight: 'bold', color: '#F1F5F9' },
  sensitivityText: { fontSize: 16, color: '#CBD5E1', marginVertical: 4 },
  sensitivityValue: { fontWeight: 'bold', color: '#3B82F6' },
  methodText: { fontSize: 12, color: '#64748B', marginTop: 8 },
});