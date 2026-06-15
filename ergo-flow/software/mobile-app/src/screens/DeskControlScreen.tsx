/**
 * ErgoFlow — Desk Control Screen
 * Desk height, monitor tilt, and ambient lighting controls
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, SliderBase } from 'react-native';
import useErgoFlowStore from '../state/ErgoFlowContext';

export default function DeskControlScreen() {
  const { deskStatus, fetchDeskStatus, setDeskHeight, setDeskPreset, setLighting, setMonitorTilt } = useErgoFlowStore();
  const [deskHeight, setDeskHeightLocal] = useState(750);
  const [monitorTilt, setMonitorTiltLocal] = useState(0);
  const [lightR, setLightR] = useState(220);
  const [lightG, setLightG] = useState(230);
  const [lightB, setLightB] = useState(255);
  const [lightW, setLightW] = useState(170);
  const [brightness, setBrightness] = useState(80);

  useEffect(() => {
    fetchDeskStatus();
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Desk Control</Text>

      {/* Desk Height Presets */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Desk Height</Text>
        <Text style={styles.currentHeight}>Current: {deskStatus.height_mm}mm</Text>

        <View style={styles.presetRow}>
          <TouchableOpacity
            style={[styles.presetButton, styles.sitButton]}
            onPress={() => { setDeskPreset('sit'); setDeskHeightLocal(720); }}
          >
            <Text style={styles.presetButtonText}>🪑 Sit</Text>
            <Text style={styles.presetSubtext}>720mm</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.presetButton, styles.standButton]}
            onPress={() => { setDeskPreset('stand'); setDeskHeightLocal(1100); }}
          >
            <Text style={styles.presetButtonText}>🧍 Stand</Text>
            <Text style={styles.presetSubtext}>1100mm</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.presetButton, styles.customButton]}
            onPress={() => setDeskPreset('custom')}
          >
            <Text style={styles.presetButtonText}>⭐ Custom</Text>
            <Text style={styles.presetSubtext}>900mm</Text>
          </TouchableOpacity>
        </View>

        <TouchableOpacity
          style={styles.stopButton}
          onPress={() => setDeskPreset('stop')}
        >
          <Text style={styles.stopButtonText}>⏹ Stop</Text>
        </TouchableOpacity>
      </View>

      {/* Monitor Tilt */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Monitor Tilt</Text>
        <Text style={styles.tiltValue}>{monitorTilt > 0 ? `+${monitorTilt}` : monitorTilt}°</Text>

        <View style={styles.tiltRow}>
          <TouchableOpacity
            style={styles.tiltButton}
            onPress={() => { setMonitorTiltLocal(monitorTilt - 3); setMonitorTilt(monitorTilt - 3); }}
          >
            <Text style={styles.tiltButtonText}>⬇ Tilt Down</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={styles.tiltButton}
            onPress={() => { setMonitorTiltLocal(0); setMonitorTilt(0); }}
          >
            <Text style={styles.tiltButtonText}>⬜ Level</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={styles.tiltButton}
            onPress={() => { setMonitorTiltLocal(monitorTilt + 3); setMonitorTilt(monitorTilt + 3); }}
          >
            <Text style={styles.tiltButtonText}>⬆ Tilt Up</Text>
          </TouchableOpacity>
        </View>
      </View>

      {/* Ambient Lighting */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Ambient Lighting</Text>

        <View style={styles.lightingPresets}>
          <TouchableOpacity
            style={styles.lightPreset}
            onPress={() => setLighting(220, 230, 255, 170, 80, 'circadian')}
          >
            <Text>☀️ Circadian</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.lightPreset}
            onPress={() => setLighting(255, 200, 150, 100, 60, 'focus')}
          >
            <Text>🎯 Focus</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.lightPreset}
            onPress={() => setLighting(120, 60, 40, 30, 15, 'relax')}
          >
            <Text>🌙 Relax</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.lightPreset}
            onPress={() => setLighting(0, 0, 0, 0, 0, 'manual')}
          >
            <Text>⚫ Off</Text>
          </TouchableOpacity>
        </View>

        <Text style={styles.sliderLabel}>Brightness: {brightness}%</Text>
      </View>
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
  cardTitle: { fontSize: 13, fontWeight: '600', color: '#6B7280', textTransform: 'uppercase', marginBottom: 8 },
  currentHeight: { fontSize: 18, color: '#374151', marginBottom: 12 },
  presetRow: { flexDirection: 'row', gap: 12, marginBottom: 12 },
  presetButton: {
    flex: 1, paddingVertical: 16, borderRadius: 12, alignItems: 'center',
  },
  sitButton: { backgroundColor: '#10B981' },
  standButton: { backgroundColor: '#4F46E5' },
  customButton: { backgroundColor: '#F59E0B' },
  presetButtonText: { color: '#FFFFFF', fontSize: 16, fontWeight: '600' },
  presetSubtext: { color: '#FFFFFF', fontSize: 12, opacity: 0.8 },
  stopButton: {
    backgroundColor: '#EF4444', borderRadius: 12, paddingVertical: 12,
    alignItems: 'center',
  },
  stopButtonText: { color: '#FFFFFF', fontSize: 16, fontWeight: '600' },
  tiltValue: { fontSize: 36, fontWeight: 'bold', color: '#4F46E5', textAlign: 'center', marginBottom: 12 },
  tiltRow: { flexDirection: 'row', gap: 12 },
  tiltButton: {
    flex: 1, backgroundColor: '#E5E7EB', borderRadius: 12,
    paddingVertical: 12, alignItems: 'center',
  },
  tiltButtonText: { fontSize: 14, fontWeight: '600', color: '#374151' },
  lightingPresets: { flexDirection: 'row', gap: 8, marginBottom: 16 },
  lightPreset: {
    flex: 1, backgroundColor: '#F3F4F6', borderRadius: 12,
    paddingVertical: 12, alignItems: 'center',
  },
  sliderLabel: { fontSize: 14, color: '#374151', marginTop: 8 },
});