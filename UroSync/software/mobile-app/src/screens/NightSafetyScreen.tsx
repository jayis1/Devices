import React from 'react';
import { Text, View } from 'react-native';
import { useUroSync } from '../services/UroSyncContext';

export default function NightSafetyScreen() {
  const state = useUroSync();
  return (
    <View style={{ backgroundColor: '#0f172a', borderRadius: 18, padding: 16, gap: 8 }}>
      <Text style={{ color: '#f8fafc', fontSize: 20, fontWeight: '600' }}>Night Safety</Text>
      <Text style={{ color: '#cbd5e1' }}>Transfer latency, sway, humidity, and guided-lighting recommendations.</Text>
      <Text style={{ color: '#93c5fd' }}>Hydration risk: {(state.hydrationRisk * 100).toFixed(0)}%</Text>
    </View>
  );
}
