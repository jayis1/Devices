/**
 * Dashboard Screen
 * Real-time posture score, current posture class, spine angle display
 */

import React, { useState, useEffect, useContext } from 'react';
import { View, Text, StyleSheet, Animated, Easing } from 'react-native';
import { PostureSyncContext } from '../services/PostureSyncContext';

const POSTURE_NAMES = [
  'Neutral', 'Forward Head', 'Slouching', 'Hyperextension',
  'Lateral Left', 'Lateral Right', 'Kyphotic', 'Lordotic',
  'Scoliotic', 'Anterior Tilt', 'Posterior Tilt', 'Crossed Legs',
];

const POSTURE_COLORS = [
  '#4CAF50', '#FF9800', '#FF5722', '#FF9800',
  '#FFC107', '#FFC107', '#F44336', '#F44336',
  '#9C27B0', '#FF9800', '#FF9800', '#FFC107',
];

export default function DashboardScreen() {
  const { postureData, postureScore } = useContext(PostureSyncContext);
  const [scoreAnim] = useState(new Animated.Value(100));
  const [ringAnim] = useState(new Animated.Value(0));

  const score = postureScore || 100;
  const postureClass = postureData?.posture_class || 0;
  const postureName = POSTURE_NAMES[postureClass] || 'Unknown';
  const postureColor = POSTURE_COLORS[postureClass] || '#757575';

  useEffect(() => {
    Animated.timing(scoreAnim, {
      toValue: score,
      duration: 500,
      easing: Easing.out(Easing.cubic),
      useNativeDriver: false,
    }).start();
  }, [score]);

  const scoreColor = score >= 80 ? '#4CAF50' : score >= 60 ? '#FF9800' : '#F44336';

  return (
    <View style={styles.container}>
      <View style={styles.scoreCard}>
        <Text style={styles.scoreLabel}>Posture Score</Text>
        <Animated.Text style={[styles.scoreValue, { color: scoreColor }]}>
          {Math.round(scoreAnim._value)}
        </Animated.Text>
        <View style={[styles.scoreRing, { borderColor: scoreColor }]} />
      </View>

      <View style={styles.postureCard}>
        <Text style={styles.postureLabel}>Current Posture</Text>
        <Text style={[styles.postureName, { color: postureColor }]}>
          {postureName}
        </Text>
      </View>

      <View style={styles.anglesCard}>
        <Text style={styles.anglesLabel}>Spinal Alignment</Text>
        <View style={styles.anglesRow}>
          <View style={styles.angleItem}>
            <Text style={styles.angleValue}>
              {postureData?.spine_angles?.pitch?.toFixed(1) || '0.0'}°
            </Text>
            <Text style={styles.angleName}>Forward Tilt</Text>
          </View>
          <View style={styles.angleItem}>
            <Text style={styles.angleValue}>
              {postureData?.spine_angles?.roll?.toFixed(1) || '0.0'}°
            </Text>
            <Text style={styles.angleName}>Lateral Tilt</Text>
          </View>
          <View style={styles.angleItem}>
            <Text style={styles.angleValue}>
              {postureData?.spine_angles?.yaw?.toFixed(1) || '0.0'}°
            </Text>
            <Text style={styles.angleName}>Rotation</Text>
          </View>
        </View>
      </View>

      <View style={styles.vitalsCard}>
        <Text style={styles.vitalsLabel}>Vitals</Text>
        <View style={styles.vitalsRow}>
          <Text style={styles.vitalItem}>❤️ {postureData?.hr || 0} bpm</Text>
          <Text style={styles.vitalItem}>🫁 {postureData?.spo2 || 0}%</Text>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, backgroundColor: '#FAFAFA' },
  scoreCard: {
    alignItems: 'center', padding: 24, backgroundColor: '#FFF',
    borderRadius: 16, marginBottom: 12, elevation: 2,
  },
  scoreLabel: { fontSize: 14, color: '#757575', marginBottom: 8 },
  scoreValue: { fontSize: 64, fontWeight: '800' },
  scoreRing: {
    position: 'absolute', width: 140, height: 140,
    borderRadius: 70, borderWidth: 4, opacity: 0.3,
  },
  postureCard: {
    alignItems: 'center', padding: 20, backgroundColor: '#FFF',
    borderRadius: 16, marginBottom: 12, elevation: 2,
  },
  postureLabel: { fontSize: 14, color: '#757575', marginBottom: 4 },
  postureName: { fontSize: 24, fontWeight: '700' },
  anglesCard: {
    padding: 20, backgroundColor: '#FFF', borderRadius: 16,
    marginBottom: 12, elevation: 2,
  },
  anglesLabel: { fontSize: 14, color: '#757575', marginBottom: 12 },
  anglesRow: { flexDirection: 'row', justifyContent: 'space-around' },
  angleItem: { alignItems: 'center' },
  angleValue: { fontSize: 28, fontWeight: '700', color: '#212121' },
  angleName: { fontSize: 12, color: '#757575', marginTop: 4 },
  vitalsCard: {
    padding: 20, backgroundColor: '#FFF', borderRadius: 16, elevation: 2,
  },
  vitalsLabel: { fontSize: 14, color: '#757575', marginBottom: 8 },
  vitalsRow: { flexDirection: 'row', justifyContent: 'space-around' },
  vitalItem: { fontSize: 18, fontWeight: '600' },
});