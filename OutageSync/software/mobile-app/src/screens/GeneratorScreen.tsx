import React from 'react';
import { Text, View } from 'react-native';

export default function GeneratorScreen() {
  return (
    <View style={{ backgroundColor: '#3f1d2e', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Generator safety</Text>
      <Text style={{ color: '#fecdd3', marginTop: 8 }}>Fuel, CO/CO₂, enclosure heat, and maintenance timers appear here before auto-start is allowed.</Text>
    </View>
  );
}
