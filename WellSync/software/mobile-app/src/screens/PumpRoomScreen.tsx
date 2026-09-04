import React from 'react';
import { Text, View } from 'react-native';

export default function PumpRoomScreen() {
  return (
    <View style={{ backgroundColor: '#0f172a', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 18, fontWeight: '600' }}>Pump room</Text>
      <Text style={{ color: '#cbd5e1', marginTop: 8 }}>Shows short-cycling score, pressure recovery, and last service event.</Text>
    </View>
  );
}
