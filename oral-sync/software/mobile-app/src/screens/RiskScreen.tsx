import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { getRisk } from '../services/api';

export default function RiskScreen() {
  const [risks, setRisks] = useState<any[]>([]);
  useEffect(() => { getRisk(1, 90).then(setRisks).catch(() => {}); }, []);

  const high = risks.filter(r => r.risk_0_100 >= 60);
  const med = risks.filter(r => r.risk_0_100 >= 30 && r.risk_0_100 < 60);
  const low = risks.filter(r => r.risk_0_100 < 30);

  return (
    <ScrollView style={s.container}>
      <Text style={s.title}>90-Day Caries Risk</Text>
      <Text style={s.hint}>Per-tooth forecast — brush & remineralize to lower risk</Text>

      <View style={s.summaryRow}>
        <View style={[s.summaryBox, { borderColor: '#f44336' }]}><Text style={s.summaryNum}>{high.length}</Text><Text style={s.summaryLbl}>High</Text></View>
        <View style={[s.summaryBox, { borderColor: '#ff9800' }]}><Text style={s.summaryNum}>{med.length}</Text><Text style={s.summaryLbl}>Medium</Text></View>
        <View style={[s.summaryBox, { borderColor: '#4caf50' }]}><Text style={s.summaryNum}>{low.length}</Text><Text style={s.summaryLbl}>Low</Text></View>
      </View>

      <Text style={s.section}>High-risk teeth</Text>
      {high.map((r, i) => (
        <View key={i} style={s.row}>
          <Text style={s.toothId}>Tooth {r.tooth_fdi} ({r.surface})</Text>
          <Text style={[s.riskVal, { color: '#f44336' }]}>{r.risk_0_100}</Text>
        </View>
      ))}
      <Text style={s.section}>Medium-risk teeth</Text>
      {med.map((r, i) => (
        <View key={i} style={s.row}>
          <Text style={s.toothId}>Tooth {r.tooth_fdi} ({r.surface})</Text>
          <Text style={[s.riskVal, { color: '#ff9800' }]}>{r.risk_0_100}</Text>
        </View>
      ))}
    </ScrollView>
  );
}

const s = StyleSheet.create({
  container: { flex: 1, padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#e3f2fd' },
  hint: { fontSize: 12, color: '#7a9ab0', marginTop: 4, marginBottom: 16 },
  summaryRow: { flexDirection: 'row', gap: 12, justifyContent: 'center', marginBottom: 20 },
  summaryBox: { borderWidth: 2, borderRadius: 12, padding: 16, alignItems: 'center', minWidth: 80 },
  summaryNum: { fontSize: 24, fontWeight: 'bold', color: '#e3f2fd' },
  summaryLbl: { fontSize: 11, color: '#7a9ab0', marginTop: 4 },
  section: { fontSize: 15, fontWeight: '600', color: '#b0c4de', marginVertical: 8 },
  row: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8, borderBottomColor: '#1a3a5c', borderBottomWidth: 1 },
  toothId: { fontSize: 14, color: '#b0c4de' },
  riskVal: { fontSize: 16, fontWeight: 'bold' },
});