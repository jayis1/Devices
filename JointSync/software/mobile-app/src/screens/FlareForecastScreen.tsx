/** JointSync — Flare Forecast Screen (stub) */
import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
export default function FlareForecastScreen() {
  return (
    <View style={s.c}><Text style={s.t}>7-Day Flare Forecast</Text>
    <Text style={s.b}>LSTM-powered 7-day flare probability with contributing factors.</Text></View>
  );
}
const s = StyleSheet.create({ c: { flex: 1, padding: 20 }, t: { fontSize: 24, fontWeight: 'bold' }, b: { fontSize: 14, color: '#666', marginTop: 10 } });