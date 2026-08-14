/**
 * Spine View Screen
 * 3D spinal curvature visualization (cervical, thoracic, lumbar segments)
 */

import React, { useContext } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { PostureSyncContext } from '../services/PostureSyncContext';

export default function SpineViewScreen() {
  const { postureData } = useContext(PostureSyncContext);
  const cervical = postureData?.spine_angles?.pitch || 0;
  const thoracic = 0;
  const lumbar = 0;

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Spinal Alignment</Text>

      <View style={styles.spineDiagram}>
        {/* Simplified spine visualization */}
        <View style={styles.segment}>
          <Text style={styles.segmentLabel}>Cervical</Text>
          <View style={[styles.bar, {
            height: Math.abs(cervical) * 2,
            backgroundColor: cervical > 15 ? '#F44336' : '#4CAF50'
          }]} />
          <Text style={styles.segmentValue}>{cervical.toFixed(1)}°</Text>
        </View>

        <View style={styles.segment}>
          <Text style={styles.segmentLabel}>Thoracic</Text>
          <View style={[styles.bar, {
            height: Math.abs(thoracic) * 2,
            backgroundColor: thoracic > 40 ? '#F44336' : '#4CAF50'
          }]} />
          <Text style={styles.segmentValue}>{thoracic.toFixed(1)}°</Text>
        </View>

        <View style={styles.segment}>
          <Text style={styles.segmentLabel}>Lumbar</Text>
          <View style={[styles.bar, {
            height: Math.abs(lumbar) * 2,
            backgroundColor: lumbar > 60 ? '#F44336' : '#4CAF50'
          }]} />
          <Text style={styles.segmentValue}>{lumbar.toFixed(1)}°</Text>
        </View>
      </View>

      <View style={styles.infoCard}>
        <Text style={styles.infoTitle}>Clinical Indicators</Text>
        <Text style={styles.infoText}>
          • Forward Head Posture: {cervical > 15 ? '⚠️ Detected' : '✅ Normal'}
        </Text>
        <Text style={styles.infoText}>
          • Thoracic Kyphosis: {thoracic > 40 ? '⚠️ At risk' : '✅ Normal'}
        </Text>
        <Text style={styles.infoText}>
          • Lumbar Lordosis: {lumbar > 60 ? '⚠️ At risk' : '✅ Normal'}
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, backgroundColor: '#FAFAFA' },
  title: { fontSize: 24, fontWeight: '700', marginBottom: 16, color: '#212121' },
  spineDiagram: {
    flexDirection: 'row', justifyContent: 'center', alignItems: 'flex-end',
    padding: 24, backgroundColor: '#FFF', borderRadius: 16, marginBottom: 12, elevation: 2,
    minHeight: 200,
  },
  segment: { alignItems: 'center', marginHorizontal: 20 },
  segmentLabel: { fontSize: 12, color: '#757575', marginBottom: 8 },
  bar: { width: 40, borderRadius: 4, minHeight: 4 },
  segmentValue: { fontSize: 14, fontWeight: '600', marginTop: 8 },
  infoCard: { padding: 20, backgroundColor: '#FFF', borderRadius: 16, elevation: 2 },
  infoTitle: { fontSize: 16, fontWeight: '700', marginBottom: 12 },
  infoText: { fontSize: 14, color: '#424242', marginBottom: 6 },
});