import React from 'react';
import { Text, View } from 'react-native';

export default function ImpactScreen() {
  return (
    <View style={{ backgroundColor: '#111827', borderRadius: 16, padding: 16, gap: 8 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Impact insights</Text>
      <Text style={{ color: '#cbd5e1' }}>Weekly landfill reduction, contamination avoided, and reusable swap opportunities.</Text>
    </View>
  );
}
