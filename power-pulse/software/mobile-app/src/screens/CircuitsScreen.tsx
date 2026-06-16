/**
 * PowerPulse — Circuits Screen (React Native)
 */

import React from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl } from 'react-native';
import { useEnergy } from '../hooks/useEnergy';
import { CircuitGauge } from '../components/CircuitGauge';

const CIRCUIT_NAMES: Record<number, string> = {
  0: 'Kitchen outlets', 1: 'Dining room', 2: 'HVAC', 3: 'Water heater',
  4: 'Oven/stove', 5: 'Dryer', 6: 'Bathroom', 7: 'Bedroom 1',
  8: 'Bedroom 2', 9: 'Living room', 10: 'Garage', 11: 'Outside',
  12: 'Fridge', 13: 'Dishwasher', 14: 'Washing machine', 15: 'Smoke detector',
};

export default function CircuitsScreen() {
  const { powerFlow, loading, refresh } = useEnergy();
  
  const circuits = powerFlow?.circuit_breakdown || {};
  const sortedCircuits = Object.entries(circuits).sort(([, a], [, b]) => b - a);
  
  return (
    <ScrollView 
      style={styles.container}
      refreshControl={<RefreshControl refreshing={loading} onRefresh={refresh} />}
    >
      <Text style={styles.title}>Per-Circuit Monitoring</Text>
      <Text style={styles.subtitle}>{sortedCircuits.length} active circuits</Text>
      
      {sortedCircuits.map(([id, watts]) => (
        <CircuitGauge
          key={id}
          circuitId={parseInt(id)}
          watts={watts}
          maxWatts={5000}
          name={CIRCUIT_NAMES[parseInt(id)] || `Circuit ${id}`}
        />
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a0a23', padding: 16 },
  title: { color: '#fff', fontSize: 24, fontWeight: '700' },
  subtitle: { color: '#888', fontSize: 14, marginTop: 4, marginBottom: 16 },
});