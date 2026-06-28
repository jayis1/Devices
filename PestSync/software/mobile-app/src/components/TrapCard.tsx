/**
 * TrapCard — Trap status card
 */
import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';

interface Props {
  trap: any;
  onReset: () => void;
}

const STATUS_COLORS: Record<string, string> = {
  armed: '#2ecc71',
  triggered: '#e74c3c',
  needs_reset: '#f39c12',
  tampered: '#9b59b6',
};

const STATUS_ICONS: Record<string, string> = {
  armed: '✅', triggered: '🎯', needs_reset: '🔄', tampered: '⚠️',
};

const CATCH_NAMES: Record<string, string> = {
  mouse: 'Mouse', rat: 'Rat', insect: 'Insect', false_trigger: 'False trigger',
};

export default function TrapCard({ trap, onReset }: Props) {
  const statusColor = STATUS_COLORS[trap.status] || '#7f8c8d';
  const statusIcon = STATUS_ICONS[trap.status] || '❓';

  return (
    <View style={styles.card}>
      <View style={styles.header}>
        <Text style={styles.name}>{trap.name}</Text>
        <Text style={[styles.status, { color: statusColor }]}>
          {statusIcon} {trap.status.replace('_', ' ').toUpperCase()}
        </Text>
      </View>

      {trap.status === 'triggered' && (
        <View style={styles.catchInfo}>
          <Text style={styles.catchText}>
            🐭 Catch: {CATCH_NAMES[trap.catch_class] || 'Unknown'} · {trap.catch_weight_g}g
          </Text>
        </View>
      )}

      <View style={styles.metricsRow}>
        <Text style={styles.metric}>🪤 Bait: {trap.bait_level}%</Text>
        <Text style={styles.metric}>🔋 {trap.battery_pct}%</Text>
      </View>

      {(trap.status === 'triggered' || trap.status === 'needs_reset') && (
        <TouchableOpacity style={styles.resetBtn} onPress={onReset}>
          <Text style={styles.resetBtnText}>Reset & Re-arm</Text>
        </TouchableOpacity>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  card: { backgroundColor: '#16213e', margin: 10, padding: 15, borderRadius: 12 },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 },
  name: { color: '#fff', fontSize: 16, fontWeight: 'bold' },
  status: { fontSize: 13, fontWeight: 'bold' },
  catchInfo: { backgroundColor: '#0f3460', padding: 8, borderRadius: 8, marginBottom: 8 },
  catchText: { color: '#fff', fontSize: 13 },
  metricsRow: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  metric: { color: '#bdc3c7', fontSize: 12 },
  resetBtn: { backgroundColor: '#e74c3c', padding: 10, borderRadius: 8, alignItems: 'center', marginTop: 5 },
  resetBtnText: { color: '#fff', fontSize: 14, fontWeight: 'bold' },
});