import React from 'react';
import { View, Text, StyleSheet, ScrollView, Switch } from 'react-native';

export default function SettingsScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Settings</Text>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Devices</Text>
        <Text style={styles.row}>TremorSync Hub — Connected</Text>
        <Text style={styles.row}>Tremor Band — Connected (85%)</Text>
        <Text style={styles.row}>Gait Pod — Connected (72%)</Text>
        <Text style={styles.row}>Voice Node — Connected (91%)</Text>
        <Text style={styles.row}>Med Station — Connected (100%)</Text>
      </View>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>FOG Cueing</Text>
        <Text style={styles.row}>Auto-cue on FOG: ON</Text>
        <Text style={styles.row}>Cueing pattern: Metronome 100 BPM</Text>
        <Text style={styles.row}>Vibration intensity: Medium</Text>
      </View>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Medication</Text>
        <Text style={styles.row}>Dose interval: 4 hours</Text>
        <Text style={styles.row}>Predictive dosing: ON</Text>
        <Text style={styles.row}>Protein advisory: ON</Text>
        <Text style={styles.row}>Caregiver alert after: 30 min</Text>
      </View>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Emergency</Text>
        <Text style={styles.row}>Fall detection: ON</Text>
        <Text style={styles.row}>Auto 911 dispatch: ON</Text>
        <Text style={styles.row}>Caregiver SMS: ON</Text>
        <Text style={styles.row}>Emergency contact: +1 805-555-1234</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50', marginBottom: 12 },
  section: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#2c3e50' },
  row: { fontSize: 14, color: '#34495e', marginBottom: 6 },
});