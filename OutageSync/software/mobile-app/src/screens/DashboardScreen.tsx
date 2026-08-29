import React from 'react';
import { Text, View } from 'react-native';
import { useOutageSync } from '../services/OutageSyncContext';

export default function DashboardScreen() {
  const state = useOutageSync();
  return (
    <View style={{ backgroundColor: '#0f172a', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 22, fontWeight: '600' }}>Forecast</Text>
      <Text style={{ color: '#93c5fd', marginTop: 8 }}>Expected outage: {state.forecast.expected_minutes} min</Text>
      <Text style={{ color: '#cbd5e1' }}>Confidence: {(state.forecast.confidence * 100).toFixed(0)}%</Text>
      <Text style={{ color: '#cbd5e1' }}>Strategy: {state.forecast.strategy}</Text>
      <Text style={{ color: '#f8fafc', marginTop: 10 }}>{state.summary}</Text>
    </View>
  );
}
