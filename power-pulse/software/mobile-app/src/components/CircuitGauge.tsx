/**
 * PowerPulse — Circuit Gauge Component
 */

import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface CircuitGaugeProps {
  circuitId: number;
  watts: number;
  maxWatts?: number;
  name?: string;
}

export function CircuitGauge({ circuitId, watts, maxWatts = 5000, name }: CircuitGaugeProps) {
  const percentage = Math.min(100, (watts / maxWatts) * 100);
  const barColor = watts > maxWatts * 0.8 ? '#ff4444' : watts > maxWatts * 0.5 ? '#ffaa00' : '#00d4aa';
  
  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.name}>{name || `Circuit ${circuitId}`}</Text>
        <Text style={styles.watts}>{watts} W</Text>
      </View>
      <View style={styles.barBackground}>
        <View style={[styles.barFill, { width: `${percentage}%`, backgroundColor: barColor }]} />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { backgroundColor: '#16213e', borderRadius: 8, padding: 12, marginBottom: 8 },
  header: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 6 },
  name: { color: '#ccc', fontSize: 14 },
  watts: { color: '#fff', fontSize: 14, fontWeight: '700' },
  barBackground: { height: 6, backgroundColor: '#333', borderRadius: 3 },
  barFill: { height: 6, borderRadius: 3 },
});