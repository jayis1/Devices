// WellnessGauge — Circular gauge showing the pet's wellness score

import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface Props {
  wellness: number;       // 0-100
  illnessRisk: number;   // 0-100
  anxietyLevel: number;  // 0-100
}

export default function WellnessGauge({ wellness, illnessRisk, anxietyLevel }: Props) {
  const getColor = (score: number) => {
    if (score >= 70) return '#4CAF50';
    if (score >= 50) return '#FF9800';
    return '#F44336';
  };

  const color = getColor(wellness);
  const circumference = 2 * Math.PI * 70;
  const progress = wellness / 100;
  const dashOffset = circumference * (1 - progress);

  return (
    <View style={styles.container}>
      <View style={[styles.gauge, { borderColor: color }]}>
        <Text style={[styles.score, { color }]}>{wellness}</Text>
        <Text style={styles.label}>WELLNESS</Text>
      </View>
      <View style={styles.metrics}>
        <View style={styles.metric}>
          <Text style={[styles.metricValue, { color: getColor(100 - illnessRisk) }]}>
            {illnessRisk}%
          </Text>
          <Text style={styles.metricLabel}>Illness Risk</Text>
        </View>
        <View style={styles.metric}>
          <Text style={[styles.metricValue, { color: getColor(100 - anxietyLevel) }]}>
            {anxietyLevel}%
          </Text>
          <Text style={styles.metricLabel}>Anxiety</Text>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    alignItems: 'center',
    padding: 20,
  },
  gauge: {
    width: 160,
    height: 160,
    borderRadius: 80,
    borderWidth: 8,
    alignItems: 'center',
    justifyContent: 'center',
  },
  score: {
    fontSize: 48,
    fontWeight: 'bold',
  },
  label: {
    fontSize: 12,
    color: '#666',
    marginTop: 4,
  },
  metrics: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    width: '100%',
    marginTop: 20,
  },
  metric: {
    alignItems: 'center',
  },
  metricValue: {
    fontSize: 24,
    fontWeight: 'bold',
  },
  metricLabel: {
    fontSize: 12,
    color: '#666',
    marginTop: 4,
  },
});