import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { listSessions, listSaliva } from '../services/api';

export default function HistoryScreen() {
  const [sessions, setSessions] = useState<any[]>([]);
  const [saliva, setSaliva] = useState<any[]>([]);
  useEffect(() => {
    listSessions(1, 30).then(setSessions).catch(() => {});
    listSaliva(1, 30).then(setSaliva).catch(() => {});
  }, []);

  return (
    <ScrollView style={s.container}>
      <Text style={s.title}>History</Text>

      <Text style={s.section}>Recent brushing sessions</Text>
      {sessions.map((se, i) => (
        <View key={se.session_id ?? i} style={s.card}>
          <Text style={s.cardDate}>{se.start}</Text>
          <View style={s.row}>
            <Text style={s.label}>{se.technique}</Text>
            <Text style={s.val}>{se.duration_s}s · {Math.round(se.coverage*100)}%</Text>
          </View>
          {se.overpressure_events > 0 ? <Text style={s.warn}>⚠ {se.overpressure_events} over-pressure events</Text> : null}
        </View>
      ))}

      <Text style={s.section}>Saliva readings</Text>
      {saliva.map((r, i) => (
        <View key={i} style={s.card}>
          <Text style={s.cardDate}>{r.ts}</Text>
          <View style={s.row}>
            <Text style={s.label}>pH {r.ph?.toFixed(2)}</Text>
            <Text style={s.val}>NO₂⁻ {r.nitrite_um?.toFixed(0)}µM · buffer {r.buffer}/5</Text>
          </View>
        </View>
      ))}
    </ScrollView>
  );
}

const s = StyleSheet.create({
  container: { flex: 1, padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#e3f2fd' },
  section: { fontSize: 15, fontWeight: '600', color: '#b0c4de', marginVertical: 12 },
  card: { backgroundColor: '#1a3a5c', borderRadius: 10, padding: 14, marginBottom: 8 },
  cardDate: { fontSize: 11, color: '#7a9ab0', marginBottom: 6 },
  row: { flexDirection: 'row', justifyContent: 'space-between' },
  label: { fontSize: 14, color: '#b0c4de' },
  val: { fontSize: 14, color: '#e3f2fd' },
  warn: { fontSize: 11, color: '#ff9800', marginTop: 6 },
});