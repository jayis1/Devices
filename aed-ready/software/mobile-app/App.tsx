import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';

export default function App() {
  return <SafeAreaView><View style={{padding: 24}}>
    <Text style={{fontSize: 24, fontWeight: '700'}}>AED cabinet status</Text>
    <Text>Prototype display — this is not a readiness certification.</Text>
    <Text>Disconnected or stale cabinets must be checked in person.</Text>
  </View></SafeAreaView>;
}
