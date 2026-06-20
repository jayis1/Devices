// StressGauge — Circular gauge showing the stress score

import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface Props {
  stress: number;       // 0-100 (100 = max stress)
  burnoutRisk: number; // 0-100
  recovery: number;    // 0-100
}

export default function StressGauge({ stress, burnoutRisk, recovery }: Props) {
  const getColor = (score: number) => {
    if (score < 30) return '#4CAF50';  // green = calm
    if (score < 50) return '#FFC107';  // yellow = normal
    if (score < 70) return '#FF9800';  // orange = elevated
    return '#F44336';                   // red = high stress
  };

  const color = getColor(stress);
  const circumference = 2 * Math.PI * 70;
  const progress = stress / 100;
  const dashOffset = circumference * (1 - progress);

  return (
    <View style={styles.container}>
      <View style={[styles.gauge, { borderColor: color }]}>
        <Text style={[styles.score, { color }]}>{stress}</Text>
        <Text style={styles.label}>STRESS</Text>
      </View>
      <View style={styles.metrics}>
        <View style={styles.metric}>
          <Text style={[styles.metricValue, { color: getColor(burnoutRisk) }]}>
            {burnoutRisk}%
          </Text>
          <Text style={styles.metricLabel}>Burnout Risk</Text>
        </View>
        <View style={styles.metric}>
          <Text style={[styles.metricValue, { color: getColor(100 - recovery) }]}>
            {recovery}%
          </Text>
          <Text style={styles.metricLabel}>Recovery</Text>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { alignItems: 'center', padding: 20 },
  gauge: {
    width: 160, height: 160, borderRadius: 80, borderWidth: 8,
    alignItems: 'center', justifyContent: 'center',
  },
  score: { fontSize: 48, fontWeight: 'bold' },
  label: { fontSize: 14, color: '#666', marginTop: 4 },
  metrics: { flexDirection: 'row', marginTop: 20, gap: 40 },
  metric: { alignItems: 'center' },
  metricValue: { fontSize: 24, fontWeight: 'bold' },
  metricLabel: { fontSize: 12, color: '#666', marginTop: 4 },
});