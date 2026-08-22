import React from 'react';
import { Text, View } from 'react-native';

export default function DashboardScreen() {
  return (
    <View style={{ backgroundColor: '#111827', borderRadius: 16, padding: 16, gap: 8 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Live diversion dashboard</Text>
      <Text style={{ color: '#cbd5e1' }}>At-a-glance diversion score, alerts, and household waste trend cards.</Text>
    </View>
  );
}
