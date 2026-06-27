import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

const PHASES = ['Mesophilic', 'Thermophilic', 'Cooling', 'Maturation', 'Cured'];
const PHASE_EMOJIS = ['🌱', '🔥', '❄️', '🍂', '✅'];

interface Props {
  phase: string;
}

export default function PhaseIndicator({ phase }: Props) {
  const currentIdx = Math.max(0, PHASES.findIndex(p => p.toLowerCase() === phase?.toLowerCase()));

  return (
    <View style={styles.container}>
      {PHASES.map((p, idx) => (
        <View key={p} style={styles.phaseItem}>
          <View style={[styles.phaseCircle, idx <= currentIdx ? styles.active : styles.inactive]}>
            <Text style={styles.emoji}>{PHASE_EMOJIS[idx]}</Text>
          </View>
          {idx < PHASES.length - 1 && (
            <View style={[styles.line, idx < currentIdx ? styles.activeLine : styles.inactiveLine]} />
          )}
          <Text style={[styles.label, idx === currentIdx ? styles.activeLabel : styles.inactiveLabel]}>{p}</Text>
        </View>
      ))}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flexDirection: 'row', justifyContent: 'space-around', paddingVertical: 16, paddingHorizontal: 8 },
  phaseItem: { alignItems: 'center', flex: 1 },
  phaseCircle: { width: 36, height: 36, borderRadius: 18, justifyContent: 'center', alignItems: 'center' },
  active: { backgroundColor: '#4CAF50' },
  inactive: { backgroundColor: '#333' },
  emoji: { fontSize: 16 },
  line: { height: 2, flex: 1, marginTop: 17 },
  activeLine: { backgroundColor: '#4CAF50' },
  inactiveLine: { backgroundColor: '#333' },
  label: { fontSize: 9, marginTop: 4, textAlign: 'center' },
  activeLabel: { color: '#4CAF50', fontWeight: 'bold' },
  inactiveLabel: { color: '#666' },
});