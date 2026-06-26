import React, { useState } from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import Svg, { Rect, G, Text as SvgText } from 'react-native-svg';

// 6 sextants: UR-buccal, UL-buccal, LR-buccal, LL-buccal, Anterior, Posterior
const SEXTANTS = ['UR-Buccal','UL-Buccal','LR-Buccal','LL-Buccal','Anterior','Posterior'];

export default function BrushSessionScreen() {
  const [active, setActive] = useState(false);
  const [coverage, setCoverage] = useState<boolean[]>(new Array(6).fill(false));
  const [technique, setTechnique] = useState('Bass');
  const [duration, setDuration] = useState(0);
  const [overpressure, setOverpressure] = useState(0);

  const start = () => { setActive(true); setCoverage(new Array(6).fill(false)); setDuration(0); setOverpressure(0); };
  const stop = () => setActive(false);

  return (
    <View style={s.container}>
      <Text style={s.title}>Brush Session</Text>
      <Text style={s.hint}>Live zone coverage from your OralSync toothbrush</Text>

      {/* Sextant coverage map */}
      <Svg width={320} height={220} style={{ alignSelf: 'center', marginTop: 20 }}>
        {SEXTANTS.map((name, i) => {
          const x = 20 + (i % 3) * 100;
          const y = 20 + Math.floor(i / 3) * 90;
          const filled = coverage[i];
          return (
            <G key={name}>
              <Rect x={x} y={y} width={80} height={70} rx={8}
                fill={filled ? '#4caf50' : '#1a3a5c'} stroke="#3a5a7c" strokeWidth={1} />
              <SvgText x={x+40} y={y+35} fontSize={9} fill="#e3f2fd" textAnchor="middle">{name}</SvgText>
              <SvgText x={x+40} y={y+50} fontSize={8} fill="#7a9ab0" textAnchor="middle">{filled?'✓':'—'}</SvgText>
            </G>
          );
        })}
      </Svg>

      <View style={s.statsRow}>
        <View style={s.statBox}>
          <Text style={s.statLabel}>Technique</Text>
          <Text style={s.statVal}>{technique}</Text>
        </View>
        <View style={s.statBox}>
          <Text style={s.statLabel}>Duration</Text>
          <Text style={s.statVal}>{Math.floor(duration/60)}:{String(duration%60).padStart(2,'0')}</Text>
        </View>
        <View style={s.statBox}>
          <Text style={s.statLabel}>Over-pressure</Text>
          <Text style={[s.statVal, { color: overpressure > 2 ? '#f44336' : '#e3f2fd' }]}>{overpressure}</Text>
        </View>
      </View>

      <TouchableOpacity style={[s.btn, active ? s.btnStop : s.btnStart]} onPress={active ? stop : start}>
        <Text style={s.btnText}>{active ? 'Stop Session' : 'Start Brushing'}</Text>
      </TouchableOpacity>
    </View>
  );
}

const s = StyleSheet.create({
  container: { flex: 1, padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#e3f2fd' },
  hint: { fontSize: 12, color: '#7a9ab0', marginTop: 4 },
  statsRow: { flexDirection: 'row', gap: 12, marginTop: 24, justifyContent: 'center' },
  statBox: { backgroundColor: '#1a3a5c', padding: 12, borderRadius: 8, alignItems: 'center', minWidth: 90 },
  statLabel: { fontSize: 10, color: '#7a9ab0' },
  statVal: { fontSize: 16, fontWeight: 'bold', color: '#e3f2fd', marginTop: 4 },
  btn: { padding: 16, borderRadius: 12, alignItems: 'center', marginTop: 24 },
  btnStart: { backgroundColor: '#4caf50' },
  btnStop: { backgroundColor: '#f44336' },
  btnText: { color: '#fff', fontSize: 16, fontWeight: 'bold' },
});