import React from 'react';
import { SafeAreaView, ScrollView, Text } from 'react-native';

// UI contract stub: static placeholder; no API transport or ASR integration.
export default function App() {
  return <SafeAreaView style={{flex: 1, padding: 24}}>
    <Text accessibilityRole="header" style={{fontSize: 24}}>CaptionBridge</Text>
    <Text>Session status: not connected</Text>
    <ScrollView accessibilityLabel="Live captions">
      <Text>Operator captions will appear here when a session is connected.</Text>
    </ScrollView>
    <Text>Prototype only — verify caption completeness with the meeting host.</Text>
  </SafeAreaView>;
}
