import React from 'react';
import { Text, View } from 'react-native';

export default function ReadinessScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Rescue Medication Readiness</Text>
      <Text>Tracks injector presence, expiry countdown, and temperature excursions.</Text>
    </View>
  );
}
