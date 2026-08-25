import React from 'react';
import { Text, View } from 'react-native';

export default function ControlsScreen() {
  return (
    <View>
      <Text>Vent Override: 20 min</Text>
      <Text>Dehumidifier Target: 47% RH</Text>
      <Text>Utility Valve: Armed</Text>
    </View>
  );
}
