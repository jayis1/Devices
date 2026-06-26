/**
 * TrailSync — Biomechanics Dashboard Screen
 *
 * Real-time gait symmetry, cadence, vertical oscillation,
 * ground contact time, impact load, pronation angle.
 * SPDX-License-Identifier: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { api } from '../api';

interface Props { runnerId: string; }

export function BiomechanicsScreen({ runnerId }: Props) {
  const [gait, setGait] = useState({
    class_name: '--', confidence: 0, cadence: 0, ground_contact: 0,
    vert_osc: 0, impact: 0, pronation: 0, asymmetry: 0, stride: 0, terrain: '--',
  });
  const [history, setHistory] = useState<any[]>([]);

  useEffect(() => {
    const fetch = async () => {
      try {
        const runner = await api.getRunner(runnerId);
        // In production: gait data comes from shoe pod via wrist unit
      } catch (e) { /* offline */ }
    };
    fetch();
    const interval = setInterval(fetch, 3000);
    return () => clearInterval(interval);
  }, [runnerId]);

  const metrics = [
    { label: 'Gait Pattern', value: gait.class_name, unit: '', color: gait.class_name === 'normal' ? '#3fb950' : '#f85149' },
    { label: 'Confidence', value: gait.confidence, unit: '%', color: '#4cc9f0' },
    { label: 'Cadence', value: gait.cadence, unit: 'spm', color: '#4cc9f0' },
    { label: 'Ground Contact', value: gait.ground_contact, unit: 'ms', color: '#4cc9f0' },
    { label: 'Vertical Oscillation', value: gait.vert_osc, unit: 'mm', color: '#4cc9f0' },
    { label: 'Impact Load', value: gait.impact, unit: '% BW', color: gait.impact > 300 ? '#f85149' : '#3fb950' },
    { label: 'Pronation', value: gait.pronation, unit: '°', color: Math.abs(gait.pronation) > 10 ? '#f85149' : '#3fb950' },
    { label: 'L/R Asymmetry', value: gait.asymmetry, unit: '%', color: gait.asymmetry > 5 ? '#f85149' : '#3fb950' },
    { label: 'Stride Length', value: gait.stride, unit: 'cm', color: '#4cc9f0' },
    { label: 'Terrain', value: gait.terrain, unit: '', color: '#8b949e' },
  ];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>🏃 Biomechanics</Text>
      <View style={styles.metricsGrid}>
        {metrics.map((m, i) => (
          <View key={i} style={styles.metricCard}>
            <Text style={styles.metricLabel}>{m.label}</Text>
            <Text style={[styles.metricValue, { color: m.color }]}>
              {m.value}{m.unit ? ` ${m.unit}` : ''}
            </Text>
          </View>
        ))}
      </View>
      <View style={styles.legend}>
        <Text style={styles.legendTitle}>Gait Patterns</Text>
        <Text style={styles.legendItem}>🟢 Normal — balanced, efficient stride</Text>
        <Text style={styles.legendItem}>🟡 Asymmetric — L/R imbalance detected</Text>
        <Text style={styles.legendItem}>🟠 Overpronating — medial pressure shift</Text>
        <Text style={styles.legendItem}>🔴 High Impact — excessive ground reaction force</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 12 },
  title: { fontSize: 24, color: '#4cc9f0', fontWeight: 'bold', marginBottom: 16 },
  metricsGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  metricCard: {
    width: '48%', backgroundColor: '#161b22', padding: 12, borderRadius: 10,
    marginBottom: 10, borderWidth: 1, borderColor: '#30363d',
  },
  metricLabel: { fontSize: 12, color: '#8b949e', marginBottom: 4 },
  metricValue: { fontSize: 22, fontWeight: 'bold' },
  legend: { backgroundColor: '#161b22', padding: 12, borderRadius: 10, marginTop: 8, borderWidth: 1, borderColor: '#30363d' },
  legendTitle: { fontSize: 14, color: '#e6edf3', fontWeight: 'bold', marginBottom: 6 },
  legendItem: { fontSize: 12, color: '#8b949e', marginBottom: 4 },
});