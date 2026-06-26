/**
 * BrewSync ScannerScreen
 * Connect to Brew Scanner via BLE for OG/FG readings, infection checks
 */
import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';

export default function ScannerScreen() {
  return (
    <View style={styles.container}>
      <Text style={styles.title}>🔬 Brew Scanner</Text>
      <Text style={styles.subtitle}>Connect your Brew Scanner to take readings</Text>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Connection Status</Text>
        <Text style={styles.status}>Not connected</Text>
        <TouchableOpacity style={styles.button}>
          <Text style={styles.buttonText}>Connect via BLE</Text>
        </TouchableOpacity>
      </View>

      <TouchableOpacity style={[styles.scanButton, styles.refractometer]}>
        <Text style={styles.scanButtonText}>🎯 Refractometer</Text>
        <Text style={styles.scanSubtext}>Measure OG / FG</Text>
      </TouchableOpacity>

      <TouchableOpacity style={[styles.scanButton, styles.infection]}>
        <Text style={styles.scanButtonText}>🦠 Infection Check</Text>
        <Text style={styles.scanSubtext}>Detect contamination</Text>
      </TouchableOpacity>

      <TouchableOpacity style={[styles.scanButton, styles.color]}>
        <Text style={styles.scanButtonText}>🎨 Color Analysis</Text>
        <Text style={styles.scanSubtext}>Measure SRM / IBU</Text>
      </TouchableOpacity>

      <TouchableOpacity style={[styles.scanButton, styles.full]}>
        <Text style={styles.scanButtonText}>📋 Full Scan</Text>
        <Text style={styles.scanSubtext}>Complete analysis</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 4 },
  subtitle: { fontSize: 14, color: '#8b949e', marginBottom: 20 },
  card: { backgroundColor: '#161b22', borderRadius: 12, padding: 16, marginBottom: 16 },
  cardTitle: { fontSize: 16, fontWeight: '600', color: '#e6e6e6', marginBottom: 8 },
  status: { fontSize: 14, color: '#8b949e', marginBottom: 12 },
  button: { backgroundColor: '#D4A017', borderRadius: 8, padding: 12, alignItems: 'center' },
  buttonText: { color: '#000', fontWeight: '600', fontSize: 14 },
  scanButton: { borderRadius: 12, padding: 20, marginBottom: 12 },
  refractometer: { backgroundColor: '#1a3a1a', borderWidth: 1, borderColor: '#2d5a2d' },
  infection: { backgroundColor: '#3a1a1a', borderWidth: 1, borderColor: '#5a2d2d' },
  color: { backgroundColor: '#1a2a3a', borderWidth: 1, borderColor: '#2d4a5a' },
  full: { backgroundColor: '#2a2a1a', borderWidth: 1, borderColor: '#5a5a2d' },
  scanButtonText: { fontSize: 18, fontWeight: '600', color: '#fff' },
  scanSubtext: { fontSize: 13, color: '#8b949e', marginTop: 4 },
});