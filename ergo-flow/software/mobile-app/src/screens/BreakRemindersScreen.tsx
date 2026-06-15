/**
 * ErgoFlow — Break Reminders Screen
 * Shows break history and stretch guides
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import React, { useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity } from 'react-native';
import useErgoFlowStore from '../state/ErgoFlowContext';

const STRETCH_GUIDES = [
  { name: 'Neck Rolls', duration: '2 min', emoji: '🔄', description: 'Slowly roll your head in a circle, 5 times each direction.' },
  { name: 'Shoulder Shrugs', duration: '1 min', emoji: '🤷', description: 'Shrug shoulders up to ears, hold 3 seconds, release. Repeat 10x.' },
  { name: 'Wrist Circles', duration: '1 min', emoji: '✋', description: 'Extend arms and rotate wrists, 10 circles each direction.' },
  { name: 'Chest Opener', duration: '2 min', emoji: '💪', description: 'Clasp hands behind back, lift and open chest. Hold 20 seconds.' },
  { name: 'Standing Extension', duration: '1 min', emoji: '🧍', description: 'Stand up, place hands on lower back, gently lean back. Hold 15 seconds.' },
  { name: 'Eye Rest (20-20-20)', duration: '2 min', emoji: '👁️', description: 'Look at something 20 feet away for 20 seconds. Every 20 minutes.' },
];

export default function BreakRemindersScreen() {
  const { breakInfo, fetchBreaks, dismissBreak } = useErgoFlowStore();

  useEffect(() => {
    fetchBreaks();
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Break Reminders</Text>

      {/* Break Stats */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Today's Progress</Text>
        <View style={styles.statsRow}>
          <View style={styles.statBox}>
            <Text style={styles.statValue}>{breakInfo.breaks_completed}</Text>
            <Text style={styles.statLabel}>Completed</Text>
          </View>
          <View style={styles.statBox}>
            <Text style={styles.statValue}>{breakInfo.breaks_today}</Text>
            <Text style={styles.statLabel}>Total</Text>
          </View>
          <View style={styles.statBox}>
            <Text style={styles.statValue}>{breakInfo.compliance_pct}%</Text>
            <Text style={styles.statLabel}>Compliance</Text>
          </View>
        </View>
        <Text style={styles.nextBreak}>
          Next break in {breakInfo.next_break_minutes} minutes
        </Text>
      </View>

      {/* Dismiss Button */}
      <TouchableOpacity style={styles.dismissButton} onPress={dismissBreak}>
        <Text style={styles.dismissButtonText}>Dismiss Current Break</Text>
      </TouchableOpacity>

      {/* Stretch Guides */}
      <Text style={styles.sectionTitle}>Stretch Guides</Text>
      {STRETCH_GUIDES.map((stretch, index) => (
        <View key={index} style={styles.stretchCard}>
          <View style={styles.stretchHeader}>
            <Text style={styles.stretchEmoji}>{stretch.emoji}</Text>
            <View style={styles.stretchInfo}>
              <Text style={styles.stretchName}>{stretch.name}</Text>
              <Text style={styles.stretchDuration}>{stretch.duration}</Text>
            </View>
          </View>
          <Text style={styles.stretchDescription}>{stretch.description}</Text>
        </View>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#F3F4F6', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#111827', marginBottom: 16 },
  card: {
    backgroundColor: '#FFFFFF', borderRadius: 12, padding: 16,
    shadowColor: '#000', shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1, shadowRadius: 3, elevation: 2, marginBottom: 16,
  },
  cardTitle: { fontSize: 13, fontWeight: '600', color: '#6B7280', textTransform: 'uppercase', marginBottom: 12 },
  statsRow: { flexDirection: 'row', justifyContent: 'space-between' },
  statBox: { alignItems: 'center', flex: 1 },
  statValue: { fontSize: 28, fontWeight: 'bold', color: '#4F46E5' },
  statLabel: { fontSize: 12, color: '#9CA3AF', marginTop: 4 },
  nextBreak: { fontSize: 14, color: '#374151', marginTop: 12, textAlign: 'center' },
  dismissButton: {
    backgroundColor: '#EF4444', borderRadius: 12, padding: 16,
    alignItems: 'center', marginBottom: 24,
  },
  dismissButtonText: { color: '#FFFFFF', fontSize: 16, fontWeight: '600' },
  sectionTitle: { fontSize: 18, fontWeight: '600', color: '#111827', marginBottom: 12 },
  stretchCard: {
    backgroundColor: '#FFFFFF', borderRadius: 12, padding: 16,
    shadowColor: '#000', shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1, shadowRadius: 3, elevation: 2, marginBottom: 8,
  },
  stretchHeader: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
  stretchEmoji: { fontSize: 28, marginRight: 12 },
  stretchInfo: { flex: 1 },
  stretchName: { fontSize: 16, fontWeight: '600', color: '#111827' },
  stretchDuration: { fontSize: 12, color: '#9CA3AF' },
  stretchDescription: { fontSize: 14, color: '#374151', lineHeight: 20 },
});