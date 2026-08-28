import React from 'react';
import { Text, View } from 'react-native';

export function DashboardScreen(): JSX.Element {
  return (
    <View style={{ padding: 14, borderWidth: 1, borderRadius: 12 }}>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Today</Text>
      <Text>Departure risk: Moderate</Text>
      <Text>Missing item candidate: Laptop</Text>
      <Text>Focus state: Stable</Text>
    </View>
  );
}
