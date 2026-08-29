import React from 'react';
import { Text, View } from 'react-native';

export default function PrepPlanScreen() {
  return (
    <View style={{ backgroundColor: '#1f2937', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Prep plan</Text>
      <Text style={{ color: '#d1d5db', marginTop: 8 }}>Shows pre-outage checklist: charge devices, make ice, precool rooms, refuel generator, and preserve battery reserve.</Text>
    </View>
  );
}
