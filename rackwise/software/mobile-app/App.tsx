import React from 'react';
import { SafeAreaView, Text, View } from 'react-native';

export default function App() {
  return <SafeAreaView><View style={{padding: 24}}>
    <Text accessibilityRole="header">RackWise</Text>
    <Text>Availability: Unknown</Text>
    <Text>Last update: Not connected</Text>
    <Text>Prototype display only — verify at the rack.</Text>
  </View></SafeAreaView>;
}
