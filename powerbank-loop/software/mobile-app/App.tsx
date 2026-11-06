import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';

export default function App() {
  return <SafeAreaView><View style={{padding: 24}}>
    <Text accessibilityRole="header">PowerBankLoop · Site overview</Text>
    <Text>Prototype scaffold — connect an authenticated operator API to show aggregate availability.</Text>
    <Text>No borrower identities or phone identifiers are collected.</Text>
  </View></SafeAreaView>;
}
