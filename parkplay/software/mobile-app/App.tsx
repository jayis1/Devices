import React from 'react';
import { SafeAreaView, Text, View } from 'react-native';

export default function App() {
  return <SafeAreaView style={{flex: 1, padding: 24, justifyContent: 'center'}}>
    <Text accessibilityRole="header" style={{fontSize: 28, fontWeight: '700'}}>ParkPlay site status</Text>
    <View accessible accessibilityLabel="No live gateway connected; readings are unknown">
      <Text style={{fontSize: 20, marginTop: 24}}>No live gateway connected</Text>
      <Text style={{fontSize: 18}}>Unknown readings require a human check. This app does not certify equipment safe.</Text>
    </View>
  </SafeAreaView>;
}
