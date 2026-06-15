/**
 * ErgoFlow — Settings Screen
 * Node management, calibration, and preferences
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import React, { useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Switch } from 'react-native';
import useErgoFlowStore from '../state/ErgoFlowContext';

export default function SettingsScreen() {
  const { nodes, fetchStatus, breakInfo } = useErgoFlowStore();
  const [circadianEnabled, setCircadianEnabled] = React.useState(true);
  const [breakEnabled, setBreakEnabled] = React.useState(true);
  const [hapticEnabled, setHapticEnabled] = React.useState(true);

  useEffect(() => {
    fetchStatus();
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Settings</Text>

      {/* Connected Nodes */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Connected Nodes</Text>
        {nodes.length > 0 ? nodes.map((node) => (
          <View key={node.id} style={styles.nodeRow}>
            <View style={styles.nodeInfo}>
              <Text style={styles.nodeName}>{node.id.replace('_', ' ').toUpperCase()}</Text>
              <Text style={styles.nodeAddr}>{node.address}</Text>
            </View>
            <View style={styles.nodeStatus}>
              <View style={[styles.statusDot, { backgroundColor: node.online ? '#10B981' : '#EF4444' }]} />
              <Text style={styles.nodeBattery}>{node.battery_pct}%</Text>
            </View>
          </View>
        )) : (
          <View style={styles.emptyNodes}>
            <Text style={styles.emptyText}>No nodes connected. Pair from the hub.</Text>
          </View>
        )}
      </View>

      {/* Preferences */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Preferences</Text>

        <View style={styles.settingRow}>
          <View>
            <Text style={styles.settingLabel}>Circadian Lighting</Text>
            <Text style={styles.settingSub}>Auto-adjust desk lighting throughout the day</Text>
          </View>
          <Switch value={circadianEnabled} onValueChange={setCircadianEnabled} />
        </View>

        <View style={styles.settingRow}>
          <View>
            <Text style={styles.settingLabel}>Break Reminders</Text>
            <Text style={styles.settingSub}>Get nudged to take breaks periodically</Text>
          </View>
          <Switch value={breakEnabled} onValueChange={setBreakEnabled} />
        </View>

        <View style={styles.settingRow}>
          <View>
            <Text style={styles.settingLabel}>Haptic Alerts</Text>
            <Text style={styles.settingSub}>Vibrate on posture warnings</Text>
          </View>
          <Switch value={hapticEnabled} onValueChange={setHapticEnabled} />
        </View>
      </View>

      {/* Calibration */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Calibration</Text>
        <TouchableOpacity style={styles.calibrateButton}>
          <Text style={styles.calibrateButtonText}>🎯 Recalibrate Pressure Sensors</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.calibrateButton}>
          <Text style={styles.calibrateButtonText}>📐 Recalibrate Desk Height</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.calibrateButton}>
          <Text style={styles.calibrateButtonText}>🧭 Recalibrate IMU</Text>
        </TouchableOpacity>
      </View>

      {/* Firmware */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Firmware Updates</Text>
        <TouchableOpacity style={styles.updateButton}>
          <Text style={styles.updateButtonText}>Check for Updates</Text>
        </TouchableOpacity>
        <Text style={styles.versionText}>Hub: v1.0.0 | Chair: v1.0.0 | Desk: v1.0.0 | Tag: v1.0.0</Text>
      </View>

      <Text style={styles.footer}>ErgoFlow v1.0.0 • Made with ❤️ by jayis1</Text>
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
  nodeRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#F3F4F6' },
  nodeInfo: { flex: 1 },
  nodeName: { fontSize: 16, fontWeight: '600', color: '#111827' },
  nodeAddr: { fontSize: 12, color: '#9CA3AF' },
  nodeStatus: { flexDirection: 'row', alignItems: 'center', gap: 8 },
  statusDot: { width: 10, height: 10, borderRadius: 5 },
  nodeBattery: { fontSize: 14, color: '#374151', fontWeight: '500' },
  emptyNodes: { paddingVertical: 24, alignItems: 'center' },
  emptyText: { fontSize: 14, color: '#9CA3AF' },
  settingRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 12, borderBottomWidth: 1, borderBottomColor: '#F3F4F6' },
  settingLabel: { fontSize: 16, fontWeight: '500', color: '#111827' },
  settingSub: { fontSize: 12, color: '#9CA3AF', marginTop: 2 },
  calibrateButton: { backgroundColor: '#EEF2FF', borderRadius: 12, padding: 16, alignItems: 'center', marginBottom: 8 },
  calibrateButtonText: { fontSize: 14, fontWeight: '600', color: '#4F46E5' },
  updateButton: { backgroundColor: '#4F46E5', borderRadius: 12, padding: 16, alignItems: 'center', marginBottom: 8 },
  updateButtonText: { fontSize: 16, fontWeight: '600', color: '#FFFFFF' },
  versionText: { fontSize: 12, color: '#9CA3AF', textAlign: 'center', marginTop: 8 },
  footer: { fontSize: 12, color: '#9CA3AF', textAlign: 'center', marginTop: 16, marginBottom: 32 },
});