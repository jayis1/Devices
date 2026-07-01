/**
 * MigraineSync — Hydration Screen
 * =================================
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, TouchableOpacity } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { getHydration, HydrationSummary, logEvent } from '../services/api';

export default function HydrationScreen() {
  const [hydration, setHydration] = useState<HydrationSummary | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetch = useCallback(async () => {
    try {
      const data = await getHydration();
      setHydration(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetch(); const i = setInterval(fetch, 60000); return () => clearInterval(i); }, [fetch]);

  const onRefresh = async () => { setRefreshing(true); await fetch(); setRefreshing(false); };

  const pct = hydration?.pct_of_goal || 0;
  const waterColor = pct >= 80 ? '#3498DB' : pct >= 50 ? '#74B9FF' : '#E17055';

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <View style={styles.header}>
        <Text style={styles.title}>Hydration</Text>
        <Text style={styles.subtitle}>Daily water intake tracking</Text>
      </View>

      {/* Water Progress */}
      <View style={styles.progressCard}>
        <Ionicons name="water" size={64} color={waterColor} />
        <Text style={styles.intakeValue}>{hydration?.intake_today_ml?.toFixed(0) || 0} ml</Text>
        <Text style={styles.goalText}>of {hydration?.goal_ml || 2000} ml goal</Text>
        <View style={styles.barContainer}>
          <View style={[styles.progressBar, { width: `${Math.min(100, pct)}%`, backgroundColor: waterColor }]} />
        </View>
        <Text style={styles.pctText}>{pct.toFixed(0)}% of goal</Text>
      </View>

      {/* Pattern Status */}
      <View style={styles.statusCard}>
        <Text style={styles.statusLabel}>Pattern</Text>
        <Text style={[styles.statusValue, { color: waterColor }]}>
          {hydration?.pattern?.toUpperCase() || '—'}
        </Text>
        <Text style={styles.recommendation}>{hydration?.recommendation || ''}</Text>
      </View>

      {/* Sip Count */}
      <View style={styles.sipCard}>
        <Ionicons name="cafe" size={28} color="#6C5CE7" />
        <View style={styles.sipInfo}>
          <Text style={styles.sipCount}>{hydration?.sip_count_today || 0}</Text>
          <Text style={styles.sipLabel}>sips today</Text>
        </View>
      </View>

      {/* 7-Day Trend */}
      {hydration?.trend_7d && hydration.trend_7d.length > 0 && (
        <View style={styles.trendCard}>
          <Text style={styles.sectionTitle}>7-Day Trend</Text>
          <View style={styles.trendBars}>
            {hydration.trend_7d.map((v, i) => (
              <View key={i} style={styles.trendBar}>
                <View style={[styles.trendBarFill, { height: `${(v / 2000) * 100}%`, backgroundColor: v >= 2000 ? '#27AE60' : v >= 1000 ? '#74B9FF' : '#E17055' }]} />
                <Text style={styles.trendBarLabel}>D{i + 1}</Text>
              </View>
            ))}
          </View>
        </View>
      )}

      {/* Manual Add */}
      <TouchableOpacity style={styles.addButton} onPress={() => logEvent('hydration', 250, 'Manual 250ml')}>
        <Ionicons name="add-circle" size={28} color="#6C5CE7" />
        <Text style={styles.addButtonText}>Add 250ml manually</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1A1A2E' },
  header: { padding: 20, paddingTop: 50 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#6C5CE7' },
  subtitle: { fontSize: 12, color: '#636E72', marginTop: 4 },
  progressCard: { margin: 20, padding: 24, borderRadius: 16, backgroundColor: '#16213E', alignItems: 'center' },
  intakeValue: { fontSize: 40, fontWeight: 'bold', color: '#FFFFFF', marginTop: 12 },
  goalText: { fontSize: 14, color: '#636E72' },
  barContainer: { width: '100%', height: 12, backgroundColor: '#0D1117', borderRadius: 6, marginTop: 16 },
  progressBar: { height: 12, borderRadius: 6 },
  pctText: { fontSize: 12, color: '#636E72', marginTop: 8 },
  statusCard: { margin: 20, padding: 16, borderRadius: 12, backgroundColor: '#16213E' },
  statusLabel: { fontSize: 12, color: '#636E72' },
  statusValue: { fontSize: 20, fontWeight: 'bold', marginTop: 4 },
  recommendation: { fontSize: 12, color: '#A0A0B0', marginTop: 8 },
  sipCard: { margin: 20, padding: 16, borderRadius: 12, backgroundColor: '#16213E', flexDirection: 'row', alignItems: 'center' },
  sipInfo: { marginLeft: 12 },
  sipCount: { fontSize: 24, fontWeight: 'bold', color: '#FFFFFF' },
  sipLabel: { fontSize: 12, color: '#636E72' },
  trendCard: { margin: 20, padding: 16, borderRadius: 12, backgroundColor: '#16213E' },
  sectionTitle: { fontSize: 14, fontWeight: '600', color: '#FFFFFF', marginBottom: 12 },
  trendBars: { flexDirection: 'row', justifyContent: 'space-between', height: 80 },
  trendBar: { flex: 1, alignItems: 'center', marginHorizontal: 4, height: '100%' },
  trendBarFill: { width: '70%', borderRadius: 4, position: 'absolute', bottom: 20 },
  trendBarLabel: { fontSize: 9, color: '#636E72', position: 'absolute', bottom: 0 },
  addButton: { flexDirection: 'row', alignItems: 'center', justifyContent: 'center', margin: 20, padding: 16, borderRadius: 12, backgroundColor: '#16213E' },
  addButtonText: { fontSize: 14, color: '#6C5CE7', marginLeft: 8 },
});