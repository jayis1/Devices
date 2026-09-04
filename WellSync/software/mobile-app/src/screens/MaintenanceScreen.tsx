import React from 'react';
import { Text, View } from 'react-native';

export default function MaintenanceScreen() {
  return (
    <View style={{ backgroundColor: '#111827', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 18, fontWeight: '600' }}>Maintenance</Text>
      <Text style={{ color: '#cbd5e1', marginTop: 8 }}>Tracks UV lamps, cartridges, lab tests, and installer notes.</Text>
    </View>
  );
}
