import React from 'react';
import { Text, View } from 'react-native';

export default function DashboardScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Today</Text>
      <Text>Leak risk in next 60 min: 18%</Text>
      <Text>Cramp forecast in next 2 h: mild rising to moderate</Text>
      <Text>Recommended action: hydrate + stage relief belt warmup</Text>
    </View>
  );
}
