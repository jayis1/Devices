import React from 'react';
import { SafeAreaView, Text, View } from 'react-native';

export default function App() {
  return <SafeAreaView style={{padding: 24}}>
    <Text style={{fontSize: 24, fontWeight: '600'}}>BatteryLoop (prototype)</Text>
    <Text>Inventory is unknown until a gateway integration is implemented.</Text>
    <View style={{marginTop: 16}}><Text>Connect a local BatteryLoop gateway to view passive observations.</Text></View>
  </SafeAreaView>;
}
