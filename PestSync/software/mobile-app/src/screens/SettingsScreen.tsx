/**
 * SettingsScreen — Device settings, calibration, profile
 */
import React from 'react';
import { View, Text, ScrollView, StyleSheet, TouchableOpacity } from 'react-native';

export default function SettingsScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Settings</Text>

      {/* Profile */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Profile</Text>
        <Text style={styles.row}>Email: demo@pestsync.local</Text>
        <Text style={styles.row}>Plan: Free</Text>
      </View>

      {/* Devices */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Devices (4)</Text>
        <Text style={styles.row}>Living Room Hub — v1.0.0 — 🔋 85%</Text>
        <Text style={styles.row}>Kitchen Sentinel — v1.0.0 — 🔋 82%</Text>
        <Text style={styles.row}>Garage Trap — v1.0.0 — 🔋 92%</Text>
        <Text style={styles.row}>Kitchen Deterrent — v1.0.0 — 🔋 85%</Text>
      </View>

      {/* Notifications */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Notifications</Text>
        <Text style={styles.row}>✅ Pest detection alerts</Text>
        <Text style={styles.row}>✅ Trap triggered alerts</Text>
        <Text style={styles.row}>✅ Infestation risk warnings</Text>
        <Text style={styles.row}>✅ Seasonal pest alerts</Text>
        <Text style={styles.row}>✅ Low battery alerts</Text>
      </View>

      {/* Actions */}
      <TouchableOpacity style={styles.btn}>
        <Text style={styles.btnText}>Add New Device</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.btn}>
        <Text style={styles.btnText}>Calibrate Sensors</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.btn}>
        <Text style={styles.btnText}>Export Infestation Report</Text>
      </TouchableOpacity>
      <TouchableOpacity style={[styles.btn, styles.btnDanger]}>
        <Text style={[styles.btnText, { color: '#e74c3c' }]}>Sign Out</Text>
      </TouchableOpacity>

      <Text style={styles.version}>PestSync v1.0.0</Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  title: { color: '#fff', fontSize: 22, fontWeight: 'bold', padding: 15 },
  card: { backgroundColor: '#16213e', margin: 10, padding: 15, borderRadius: 12 },
  cardTitle: { color: '#e0e0e0', fontSize: 16, fontWeight: 'bold', marginBottom: 8 },
  row: { color: '#bdc3c7', fontSize: 13, marginBottom: 5 },
  btn: { backgroundColor: '#16213e', margin: 10, padding: 15, borderRadius: 12, alignItems: 'center' },
  btnText: { color: '#fff', fontSize: 16 },
  btnDanger: { backgroundColor: '#1a1a2e', borderWidth: 1, borderColor: '#e74c3c' },
  version: { color: '#7f8c8d', fontSize: 12, textAlign: 'center', marginTop: 20, marginBottom: 20 },
});