import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function SettingsScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Settings</Text>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Devices</Text>
        <Text style={styles.deviceRow}>● Hub — Connected (85%)</Text>
        <Text style={styles.deviceRow}>● Smart Helmet — Connected (72%)</Text>
        <Text style={styles.deviceRow}>● Smart Light — Connected (90%)</Text>
        <Text style={styles.deviceRow}>● Bike Sensor — Connected (88%)</Text>
        <Text style={styles.deviceRow}>● Smart Lock — Connected (95%)</Text>
      </View>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Emergency Contact</Text>
        <Text style={styles.deviceRow}>+1 805-555-1234</Text>
      </View>
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Crash Auto-Dispatch</Text>
        <Text style={styles.deviceRow}>Enabled — 10s confirmation window</Text>
      </View>
    </ScrollView>
  );
}
const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 20 },
  section: { backgroundColor: '#1a1a1a', padding: 16, borderRadius: 10, marginBottom: 10 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', color: '#2ecc71', marginBottom: 8 },
  deviceRow: { fontSize: 14, color: '#fff', paddingVertical: 4 },
});