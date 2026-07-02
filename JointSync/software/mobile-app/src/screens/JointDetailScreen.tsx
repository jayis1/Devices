/** JointSync — Joint Detail Screen (stub) */
import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
export default function JointDetailScreen() {
  return (
    <View style={s.c}><Text style={s.t}>Joint Detail</Text>
    <Text style={s.b}>Per-joint ROM chart, temperature chart, thermal scans, therapy log.</Text></View>
  );
}
const s = StyleSheet.create({ c: { flex: 1, padding: 20 }, t: { fontSize: 24, fontWeight: 'bold' }, b: { fontSize: 14, color: '#666', marginTop: 10 } });