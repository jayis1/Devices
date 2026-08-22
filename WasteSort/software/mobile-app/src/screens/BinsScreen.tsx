import React from 'react';
import { Text, View } from 'react-native';

export default function BinsScreen() {
  return (
    <View style={{ backgroundColor: '#111827', borderRadius: 16, padding: 16, gap: 8 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Bin health</Text>
      <Text style={{ color: '#cbd5e1' }}>Fill, mass, odor index, and battery state for every indoor waste stream.</Text>
    </View>
  );
}
