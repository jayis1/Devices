import React from 'react';
import { SafeAreaView, Text, Button, AccessibilityInfo } from 'react-native';

export default function App() {
  return <SafeAreaView accessibilityLabel="PawStation site status">
    <Text accessibilityRole="header">PawStation</Text>
    <Text>Station status is advisory. Staff must inspect in person.</Text>
    <Button title="Record service confirmed" onPress={() => AccessibilityInfo.announceForAccessibility('Service confirmation is a demo only')} />
  </SafeAreaView>;
}
