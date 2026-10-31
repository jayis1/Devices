import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';

export default function App() {
  return <SafeAreaView><View accessibilityLabel="ArtPath information">
    <Text accessibilityRole="header">ArtPath</Text>
    <Text>Use the artwork station's NFC marker or large button for locally cached, curator-approved descriptions.</Text>
    <Text>This prototype app does not yet discover stations or fetch content.</Text>
  </View></SafeAreaView>;
}
