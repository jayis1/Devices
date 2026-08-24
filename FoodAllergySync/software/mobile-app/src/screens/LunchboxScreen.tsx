import React from 'react';
import { Text, View } from 'react-native';

export default function LunchboxScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>SafeLunch Beacon</Text>
      <Text>Displays temperature curve, lid events, and approved container check.</Text>
    </View>
  );
}
