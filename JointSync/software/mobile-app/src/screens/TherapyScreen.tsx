/** JointSync — Therapy Screen (stub) */
import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
export default function TherapyScreen() {
  return (
    <View style={s.c}><Text style={s.t}>Compression Therapy</Text>
    <Text style={s.b}>Sleeve control, therapy schedule, adherence stats.</Text></View>
  );
}
const s = StyleSheet.create({ c: { flex: 1, padding: 20 }, t: { fontSize: 24, fontWeight: 'bold' }, b: { fontSize: 14, color: '#666', marginTop: 10 } });