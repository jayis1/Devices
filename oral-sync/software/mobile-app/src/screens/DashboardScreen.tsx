import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import Svg, { Circle, Path, G, Text as SvgText } from 'react-native-svg';
import { listUsers, getRisk, listSaliva, openStream } from '../services/api';

interface ToothRisk { tooth_fdi: number; surface: string; risk_0_100: number; }

export default function DashboardScreen() {
  const [users, setUsers] = useState<any[]>([]);
  const [risks, setRisks] = useState<ToothRisk[]>([]);
  const [saliva, setSaliva] = useState<any | null>(null);
  const [liveEvent, setLiveEvent] = useState<string>('');

  useEffect(() => {
    listUsers().then(setUsers).catch(() => {});
    if (users[0]) {
      getRisk(users[0].user_id).then(setRisks).catch(() => {});
      listSaliva(users[0].user_id, 1).then((d: any) => setSaliva(d[0])).catch(() => {});
    }
    const ws = openStream((evt) => setLiveEvent(JSON.stringify(evt)));
    return () => ws.close();
  }, [users[0]?.user_id]);

  const avgRisk = risks.length ? Math.round(risks.reduce((a, t) => a + t.risk_0_100, 0) / risks.length) : 0;
  const riskColor = avgRisk < 30 ? '#4caf50' : avgRisk < 60 ? '#ff9800' : '#f44336';

  // FDI tooth map: upper-right 18-11, upper-left 21-28, lower-left 31-38, lower-right 41-48
  const upperTeeth = [18,17,16,15,14,13,12,11,21,22,23,24,25,26,27,28];
  const lowerTeeth = [48,47,46,45,44,43,42,41,31,32,33,34,35,36,37,38];

  const toothRisk = (fdi: number) => {
    const t = risks.find(r => r.tooth_fdi === fdi);
    return t ? t.risk_0_100 : 0;
  };
  const toothColor = (fdi: number) => {
    const r = toothRisk(fdi);
    if (r < 20) return '#4caf50';
    if (r < 40) return '#cddc39';
    if (r < 60) return '#ff9800';
    return '#f44336';
  };

  return (
    <ScrollView style={s.container}>
      <Text style={s.title}>OralSync</Text>
      <Text style={s.subtitle}>Whole-mouth oral health intelligence</Text>

      {/* Risk gauge */}
      <View style={s.gaugeRow}>
        <Svg width={120} height={120}>
          <Circle cx={60} cy={60} r={50} stroke="#1a3a5c" strokeWidth={10} fill="none" />
          <Circle cx={60} cy={60} r={50} stroke={riskColor} strokeWidth={10} fill="none"
            strokeDasharray={`${(avgRisk/100)*314} 314`} rotation="-90" origin="60,60" />
          <SvgText x={60} y={65} fontSize={28} fill={riskColor} textAnchor="middle">{avgRisk}</SvgText>
          <SvgText x={60} y={85} fontSize={11} fill="#7a9ab0" textAnchor="middle">90-day risk</SvgText>
        </Svg>
        <View style={s.gaugeSide}>
          <Text style={s.metricLabel}>Salivary pH</Text>
          <Text style={[s.metricVal, { color: (saliva?.ph ?? 6.8) < 6.2 ? '#f44336' : '#4caf50' }]}>
            {(saliva?.ph ?? 6.8).toFixed(2)}
          </Text>
          <Text style={s.metricLabel}>Nitrite (µM)</Text>
          <Text style={s.metricVal}>{saliva?.nitrite_um?.toFixed(0) ?? '—'}</Text>
          <Text style={s.metricLabel}>Buffer capacity</Text>
          <Text style={s.metricVal}>{saliva?.buffer ?? '—'}/5</Text>
        </View>
      </View>

      {/* Tooth map */}
      <Text style={s.section}>Tooth risk map (FDI)</Text>
      <View style={s.toothRow}>
        {upperTeeth.map(fdi => (
          <View key={fdi} style={[s.tooth, { backgroundColor: toothColor(fdi) }]}>
            <Text style={s.toothNum}>{fdi}</Text>
          </View>
        ))}
      </View>
      <View style={s.toothRow}>
        {lowerTeeth.map(fdi => (
          <View key={fdi} style={[s.tooth, { backgroundColor: toothColor(fdi) }]}>
            <Text style={s.toothNum}>{fdi}</Text>
          </View>
        ))}
      </View>

      {liveEvent ? (
        <View style={s.liveBox}>
          <Text style={s.liveLabel}>LIVE</Text>
          <Text style={s.liveText}>{liveEvent}</Text>
        </View>
      ) : null}
    </ScrollView>
  );
}

const s = StyleSheet.create({
  container: { flex: 1, padding: 16 },
  title: { fontSize: 28, fontWeight: 'bold', color: '#e3f2fd' },
  subtitle: { fontSize: 13, color: '#7a9ab0', marginBottom: 16 },
  gaugeRow: { flexDirection: 'row', alignItems: 'center', gap: 24, marginBottom: 20 },
  gaugeSide: { gap: 2 },
  metricLabel: { fontSize: 11, color: '#5a7a9a' },
  metricVal: { fontSize: 18, fontWeight: 'bold', color: '#e3f2fd', marginBottom: 6 },
  section: { fontSize: 15, fontWeight: '600', color: '#b0c4de', marginVertical: 8 },
  toothRow: { flexDirection: 'row', flexWrap: 'wrap', gap: 4, marginBottom: 4 },
  tooth: { width: 18, height: 24, borderRadius: 3, justifyContent: 'center', alignItems: 'center' },
  toothNum: { fontSize: 7, color: '#0a1929', fontWeight: 'bold' },
  liveBox: { marginTop: 16, padding: 10, backgroundColor: '#1a3a5c', borderRadius: 8 },
  liveLabel: { fontSize: 10, color: '#4fc3f7', fontWeight: 'bold' },
  liveText: { fontSize: 11, color: '#b0c4de', marginTop: 4 },
});