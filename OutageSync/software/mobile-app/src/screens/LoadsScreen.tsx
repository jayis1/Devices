import React from 'react';
import { Text, View } from 'react-native';
import { useOutageSync } from '../services/OutageSyncContext';

export default function LoadsScreen() {
  const state = useOutageSync();
  return (
    <View style={{ backgroundColor: '#111827', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Critical loads</Text>
      {state.decisions.map((d) => (
        <View key={d.label} style={{ marginTop: 10 }}>
          <Text style={{ color: '#f8fafc' }}>{d.label}: {d.action}</Text>
          <Text style={{ color: '#9ca3af' }}>{d.rationale}</Text>
        </View>
      ))}
    </View>
  );
}
