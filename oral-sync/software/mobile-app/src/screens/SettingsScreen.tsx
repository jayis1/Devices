import React from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity } from 'react-native';
import { generateReport } from '../services/api';

export default function SettingsScreen() {
  return (
    <ScrollView style={s.container}>
      <Text style={s.title}>Settings</Text>

      <Text style={s.section}>Household</Text>
      <Text style={s.item}>Ada (34) — Standard mode</Text>
      <Text style={s.item}>Leo (12) — Orthodontic mode (braces)</Text>

      <Text style={s.section}>Brushing goals</Text>
      <Text style={s.item}>2 minutes, 2× daily</Text>
      <Text style={s.item}>Target plaque coverage reduction: 50%</Text>

      <Text style={s.section}>Consumables</Text>
      <Text style={s.item}>pH tip: 18 uses left</Text>
      <Text style={s.item}>Nitrite tip: 12 uses left</Text>
      <Text style={s.item}>Scanner lens covers: 7 left</Text>

      <Text style={s.section}>Dentist</Text>
      <TouchableOpacity style={s.btn} onPress={() => generateReport(1)}>
        <Text style={s.btnText}>Generate dentist report (PDF)</Text>
      </TouchableOpacity>

      <Text style={s.section}>Privacy</Text>
      <Text style={s.item}>Images encrypted at rest (KMS)</Text>
      <Text style={s.item}>Embeddings-only mode: Off</Text>
      <Text style={s.item}>HIPAA-aware data handling</Text>

      <Text style={s.footer}>OralSync v1.0 · wellness device (not a medical device)</Text>
    </ScrollView>
  );
}

const s = StyleSheet.create({
  container: { flex: 1, padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#e3f2fd' },
  section: { fontSize: 15, fontWeight: '600', color: '#b0c4de', marginTop: 16, marginBottom: 6 },
  item: { fontSize: 13, color: '#b0c4de', paddingVertical: 3 },
  btn: { backgroundColor: '#4fc3f7', padding: 14, borderRadius: 10, marginTop: 8 },
  btnText: { color: '#0a1929', fontSize: 14, fontWeight: 'bold', textAlign: 'center' },
  footer: { fontSize: 11, color: '#5a7a9a', marginTop: 24, textAlign: 'center' },
});