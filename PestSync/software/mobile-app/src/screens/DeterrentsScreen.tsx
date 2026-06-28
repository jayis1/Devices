/**
 * DeterrentsScreen — Deterrent control & schedule
 */
import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet, TouchableOpacity, Switch } from 'react-native';
import { fetchDeterrents, sendDeterrentCommand } from '../api/client';

export default function DeterrentsScreen() {
  const [deterrents, setDeterrents] = useState<any[]>([]);

  useEffect(() => {
    fetchDeterrents().then(setDeterrents).catch(() => {});
  }, []);

  const toggleMode = (id: string, currentMode: string) => {
    const newMode = currentMode === 'adaptive' ? 'off' : 'adaptive';
    sendDeterrentCommand(id, { mode: newMode });
    setDeterrents(deterrents.map(d => d.device_id === id ? { ...d, mode: newMode } : d));
  };

  const triggerStrobe = (id: string) => {
    sendDeterrentCommand(id, { action: 'strobe' });
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Deterrents</Text>

      {deterrents.map((d) => (
        <View key={d.device_id} style={styles.card}>
          <View style={styles.header}>
            <Text style={styles.name}>{d.name}</Text>
            <Switch
              value={d.mode !== 'off'}
              onValueChange={() => toggleMode(d.device_id, d.mode)}
              trackColor={{ false: '#333', true: '#e74c3c' }}
            />
          </View>

          {/* Mode display */}
          <Text style={styles.mode}>Mode: {d.mode} · Band: {d.band}</Text>

          {/* Status indicators */}
          <View style={styles.statusRow}>
            <View style={[styles.statusPill, d.ultrasonic_active && styles.pillActive]}>
              <Text style={styles.pillText}>🔊 Ultrasonic</Text>
            </View>
            <View style={[styles.statusPill, d.strobe_active && styles.pillActive]}>
              <Text style={styles.pillText}>💡 Strobe</Text>
            </View>
            <View style={[styles.statusPill, d.diffuser_active && styles.pillActive]}>
              <Text style={styles.pillText}>🌿 Diffuser</Text>
            </View>
          </View>

          {/* Oil level */}
          <Text style={styles.oilLevel}>Oil: {d.oil_level}% {d.oil_level < 20 ? '⚠️ Refill needed' : '✅'}</Text>

          {/* Effectiveness */}
          <Text style={styles.effectiveness}>
            Effectiveness: {d.total_ultrasonic_s}s ultrasonic · {d.diffuser_doses} doses
          </Text>

          {/* Quick actions */}
          <View style={styles.actionRow}>
            <TouchableOpacity style={styles.actionBtn} onPress={() => triggerStrobe(d.device_id)}>
              <Text style={styles.actionBtnText}>⚡ Strobe</Text>
            </TouchableOpacity>
            <TouchableOpacity style={styles.actionBtn} onPress={() => sendDeterrentCommand(d.device_id, { action: 'diffuse' })}>
              <Text style={styles.actionBtnText}>🌿 Diffuse</Text>
            </TouchableOpacity>
          </View>
        </View>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  title: { color: '#fff', fontSize: 22, fontWeight: 'bold', padding: 15 },
  card: { backgroundColor: '#16213e', margin: 10, padding: 15, borderRadius: 12 },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 },
  name: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  mode: { color: '#95a5a6', fontSize: 13, marginBottom: 10 },
  statusRow: { flexDirection: 'row', justifyContent: 'space-around', marginBottom: 10 },
  statusPill: { backgroundColor: '#0f3460', padding: 8, borderRadius: 16, minWidth: 90, alignItems: 'center' },
  pillActive: { backgroundColor: '#e74c3c' },
  pillText: { color: '#fff', fontSize: 12 },
  oilLevel: { color: '#bdc3c7', fontSize: 13, marginBottom: 5 },
  effectiveness: { color: '#7f8c8d', fontSize: 12, marginBottom: 10 },
  actionRow: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 5 },
  actionBtn: { backgroundColor: '#0f3460', padding: 10, borderRadius: 8, minWidth: 100, alignItems: 'center' },
  actionBtnText: { color: '#fff', fontSize: 14 },
});