import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';
export default function App() {
  return <SafeAreaView><View style={{padding:24}}><Text>RainBarrel Sync — prototype</Text><Text>Connect to a configured local gateway to show readings.</Text><Text>Unavailable or stale readings are not represented as zero.</Text></View></SafeAreaView>;
}
