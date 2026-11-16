import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';

export default function App() {
  return <SafeAreaView><View style={{padding: 24}}>
    <Text style={{fontSize: 24}}>GreaseLoop</Text>
    <Text>Prototype UI scaffold — no device pairing or live data.</Text>
    <Text>Estimated container mass: unavailable</Text>
    <Text>Cool oil fully, seal the container, and check local collection rules.</Text>
  </View></SafeAreaView>;
}
