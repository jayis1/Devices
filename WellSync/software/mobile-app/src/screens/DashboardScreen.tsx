import React from 'react';
import { Text, View } from 'react-native';
import { useWellSync } from '../services/WellSyncContext';

export default function DashboardScreen() {
  const state = useWellSync();
  return (
    <View style={{ backgroundColor: '#111827', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Household state: {state.state}</Text>
      <Text style={{ color: '#93c5fd', marginTop: 8 }}>Contamination risk: {Math.round(state.risks.contamination * 100)}%</Text>
      <Text style={{ color: '#93c5fd' }}>Pump risk: {Math.round(state.risks.pumpFailure * 100)}%</Text>
      <Text style={{ color: '#93c5fd' }}>Dry-well risk: {Math.round(state.risks.dryWell * 100)}%</Text>
    </View>
  );
}
