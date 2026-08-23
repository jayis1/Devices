import React from 'react';
import { Text, View } from 'react-native';

export default function ReliefScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Relief Control</Text>
      <Text>Preset A: 40.5°C + gentle haptics</Text>
      <Text>Preset B: 42.0°C + stronger pulse</Text>
      <Text>Safety lockouts displayed locally from the hub</Text>
    </View>
  );
}
