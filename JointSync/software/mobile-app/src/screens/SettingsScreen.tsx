/** JointSync — Settings Screen (stub) */
import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
export default function SettingsScreen() {
  return (
    <View style={s.c}><Text style={s.t}>Settings</Text>
    <Text style={s.b}>Tag pairing, joint config, notification prefs.</Text></View>
  );
}
const s = StyleSheet.create({ c: { flex: 1, padding: 20 }, t: { fontSize: 24, fontWeight: 'bold' }, b: { fontSize: 14, color: '#666', marginTop: 10 } });