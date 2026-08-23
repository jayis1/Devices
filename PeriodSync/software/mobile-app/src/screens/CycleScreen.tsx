import React from 'react';
import { Text, View } from 'react-native';

export default function CycleScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Cycle Forecast</Text>
      <Text>Phase confidence: high</Text>
      <Text>Estimated next period window: 2 days</Text>
      <Text>LH strip trend: rising</Text>
    </View>
  );
}
