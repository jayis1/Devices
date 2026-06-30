/**
 * AsthmaSync — Action Plan Screen
 * GINA-aligned zone-based action plan.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, TouchableOpacity } from 'react-native';
import { ApiClient } from '../services/api';

interface ActionPlan {
  zone: string;
  rescue_use_7d: number;
  last_spo2: number;
  steps: string[];
  last_updated: string;
}

export default function ActionPlanScreen() {
  const [plan, setPlan] = useState<ActionPlan | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetchPlan = useCallback(async () => {
    const api = ApiClient.getInstance();
    try {
      const data = await api.getActionPlan();
      setPlan(data);
    } catch (e) {
      console.error('Action plan fetch error:', e);
    }
  }, []);

  useEffect(() => {
    fetchPlan();
  }, [fetchPlan]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchPlan();
    setRefreshing(false);
  }, [fetchPlan]);

  const zoneConfig: Record<string, { color: string; bg: string; icon: string; title: string }> = {
    green:  { color: '#2E7D32', bg: '#E8F5E9', icon: '✅', title: 'Green Zone — Well Controlled' },
    yellow: { color: '#F57F17', bg: '#FFF8E1', icon: '⚠️', title: 'Yellow Zone — Caution' },
    red:    { color: '#C62828', bg: '#FFEBEE', icon: '🚨', title: 'Red Zone — Medical Alert' },
  };

  const zone = plan?.zone || 'green';
  const config = zoneConfig[zone] || zoneConfig.green;

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Zone Banner */}
      <View style={[styles.zoneBanner, { backgroundColor: config.bg, borderColor: config.color }]}>
        <Text style={styles.zoneIcon}>{config.icon}</Text>
        <Text style={[styles.zoneTitle, { color: config.color }]}>{config.title}</Text>
      </View>

      {/* Current Status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Current Status</Text>
        <View style={styles.statusRow}>
          <Text style={styles.statusLabel}>Rescue uses (7 days):</Text>
          <Text style={[styles.statusValue, { color: (plan?.rescue_use_7d || 0) > 4 ? '#C62828' :
                                          (plan?.rescue_use_7d || 0) > 2 ? '#F57F17' : '#2E7D32' }]}>
            {plan?.rescue_use_7d ?? '--'}
          </Text>
        </View>
        <View style={styles.statusRow}>
          <Text style={styles.statusLabel}>Latest SpO₂:</Text>
          <Text style={[styles.statusValue, { color: (plan?.last_spo2 || 100) < 92 ? '#C62828' :
                                          (plan?.last_spo2 || 100) < 95 ? '#F57F17' : '#2E7D32' }]}>
            {plan?.last_spo2 ? `${plan.last_spo2}%` : '--'}
          </Text>
        </View>
      </View>

      {/* Action Steps */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>What to Do Now</Text>
        {plan?.steps.map((step, i) => (
          <View key={i} style={styles.stepRow}>
            <Text style={[styles.stepNumber, { color: config.color }]}>{i + 1}</Text>
            <Text style={styles.stepText}>{step}</Text>
          </View>
        ))}
      </View>

      {/* Emergency */}
      <View style={[styles.card, { borderColor: '#C62828' }]}>
        <Text style={[styles.cardTitle, { color: '#C62828' }]}>Emergency</Text>
        <Text style={styles.emergencyText}>
          If severe breathlessness, can't speak in full sentences, or lips turn blue:
        </Text>
        <TouchableOpacity style={styles.emergencyButton}>
          <Text style={styles.emergencyButtonText}>Call 911 / 112 / 999</Text>
        </TouchableOpacity>
      </View>

      {/* Last Updated */}
      <Text style={styles.lastUpdated}>
        Last updated: {plan ? new Date(plan.last_updated).toLocaleString() : '—'}
      </Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f0f4f8' },
  zoneBanner: {
    borderRadius: 12,
    padding: 20,
    margin: 12,
    borderWidth: 2,
    alignItems: 'center',
  },
  zoneIcon: { fontSize: 48, marginBottom: 8 },
  zoneTitle: { fontSize: 18, fontWeight: 'bold', textAlign: 'center' },
  card: { backgroundColor: '#fff', borderRadius: 12, padding: 16, margin: 12, borderWidth: 1, borderColor: '#e0e0e0' },
  cardTitle: { fontSize: 18, fontWeight: 'bold', color: '#333', marginBottom: 12 },
  statusRow: { flexDirection: 'row', justifyContent: 'space-between', marginVertical: 6 },
  statusLabel: { fontSize: 14, color: '#555' },
  statusValue: { fontSize: 16, fontWeight: 'bold' },
  stepRow: { flexDirection: 'row', marginVertical: 8, alignItems: 'flex-start' },
  stepNumber: { fontSize: 18, fontWeight: 'bold', marginRight: 12, minWidth: 24 },
  stepText: { fontSize: 14, color: '#333', flex: 1, lineHeight: 20 },
  emergencyText: { fontSize: 14, color: '#555', marginBottom: 12, lineHeight: 20 },
  emergencyButton: {
    backgroundColor: '#C62828',
    borderRadius: 8,
    padding: 14,
    alignItems: 'center',
  },
  emergencyButtonText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  lastUpdated: { fontSize: 11, color: '#999', textAlign: 'center', margin: 12 },
});