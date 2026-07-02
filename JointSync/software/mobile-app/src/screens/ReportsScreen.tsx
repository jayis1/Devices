/** JointSync — Reports Screen (stub) */
import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
export default function ReportsScreen() {
  return (
    <View style={s.c}><Text style={s.t}>Clinical Reports</Text>
    <Text style={s.b}>Generate and share rheumatologist report (PDF).</Text></View>
  );
}
const s = StyleSheet.create({ c: { flex: 1, padding: 20 }, t: { fontSize: 24, fontWeight: 'bold' }, b: { fontSize: 14, color: '#666', marginTop: 10 } });