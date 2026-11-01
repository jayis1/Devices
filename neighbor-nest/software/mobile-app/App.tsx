import React from 'react';
import { SafeAreaView, Text, View } from 'react-native';

export default function App() {
  return <SafeAreaView style={{padding: 24}}>
    <Text style={{fontSize: 26, fontWeight: '700'}}>NeighborNest</Text>
    <Text>Common room · demo data · not connected</Text>
    <View style={{marginTop: 24}}>
      <Text>Window planter — moisture 31% · check today</Text>
      <Text>Library shelf — moisture 58% · no action</Text>
      <Text>Last volunteer visit: sample only</Text>
    </View>
  </SafeAreaView>;
}
