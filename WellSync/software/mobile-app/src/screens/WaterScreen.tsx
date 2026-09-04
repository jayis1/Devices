import React from 'react';
import { Text, View } from 'react-native';
import { useWellSync } from '../services/WellSyncContext';

export default function WaterScreen() {
  const state = useWellSync();
  return (
    <View style={{ backgroundColor: '#0f172a', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 18, fontWeight: '600' }}>Chemistry</Text>
      <Text style={{ color: '#cbd5e1', marginTop: 8 }}>pH: {state.latestPh.toFixed(2)}</Text>
      <Text style={{ color: '#cbd5e1' }}>Turbidity: {state.latestTurbidity.toFixed(1)} NTU</Text>
      <Text style={{ color: '#cbd5e1' }}>Pressure: {state.pressureKpa} kPa</Text>
    </View>
  );
}
