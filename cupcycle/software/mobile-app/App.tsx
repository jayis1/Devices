import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';

export default function App() {
  return <SafeAreaView><View style={{padding: 24}}>
    <Text accessibilityRole="header">CupCycle</Text>
    <Text>Prototype status: connect to a secured site API to view live data.</Text>
    <Text>Wash readiness is staff-confirmed, not measured by this app.</Text>
    <Text>Offline or stale data must be reconciled in person.</Text>
  </View></SafeAreaView>;
}
