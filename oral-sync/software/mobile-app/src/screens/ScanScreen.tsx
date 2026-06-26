import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, ScrollView, Image } from 'react-native';
import { listScans } from '../services/api';

export default function ScanScreen() {
  const [scans, setScans] = useState<any[]>([]);
  useEffect(() => { listScans(1).then(setScans).catch(() => {}); }, []);

  return (
    <ScrollView style={s.container}>
      <Text style={s.title}>Plaque Scans</Text>
      <Text style={s.hint}>Multispectral intraoral imaging — 405/470/525/660/850 nm</Text>
      {scans.map((sc, i) => (
        <View key={sc.scan_id ?? i} style={s.card}>
          <Text style={s.cardDate}>{sc.ts}</Text>
          <View style={s.cardRow}>
            <Text style={s.cardLabel}>Plaque coverage</Text>
            <Text style={[s.cardVal, { color: sc.plaque_pct > 30 ? '#f44336' : '#4caf50' }]}>
              {sc.plaque_pct?.toFixed(1)}%
            </Text>
          </View>
          <View style={s.cardRow}>
            <Text style={s.cardLabel}>Lesions detected</Text>
            <Text style={[s.cardVal, { color: sc.lesions > 0 ? '#ff9800' : '#4caf50' }]}>{sc.lesions}</Text>
          </View>
        </View>
      ))}
      <Text style={s.tip}>Tip: scan weekly to track lesion changes over time.</Text>
    </ScrollView>
  );
}

const s = StyleSheet.create({
  container: { flex: 1, padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#e3f2fd' },
  hint: { fontSize: 12, color: '#7a9ab0', marginTop: 4, marginBottom: 16 },
  card: { backgroundColor: '#1a3a5c', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardDate: { fontSize: 12, color: '#7a9ab0', marginBottom: 8 },
  cardRow: { flexDirection: 'row', justifyContent: 'space-between', marginTop: 4 },
  cardLabel: { fontSize: 14, color: '#b0c4de' },
  cardVal: { fontSize: 16, fontWeight: 'bold', color: '#e3f2fd' },
  tip: { fontSize: 12, color: '#5a7a9a', marginTop: 16, textAlign: 'center' },
});