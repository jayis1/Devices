import React from 'react';
import { Text, View } from 'react-native';

export default function ColdChainScreen() {
  return (
    <View style={{ backgroundColor: '#082f49', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Cold-chain safety</Text>
      <Text style={{ color: '#dbeafe', marginTop: 8 }}>Fridge, freezer, and medicine hold-time cards would render here with push-alert thresholds.</Text>
    </View>
  );
}
