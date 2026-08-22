import React from 'react';
import { Text, View } from 'react-native';

export default function PickupScreen() {
  return (
    <View style={{ backgroundColor: '#111827', borderRadius: 16, padding: 16, gap: 8 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Pickup readiness</Text>
      <Text style={{ color: '#cbd5e1' }}>Curb reminders, service verification, and missed-pickup risk forecasts.</Text>
    </View>
  );
}
