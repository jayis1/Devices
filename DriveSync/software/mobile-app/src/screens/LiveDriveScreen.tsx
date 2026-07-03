/**
 * Live Drive Screen — real-time risk gauge during driving
 * License: MIT
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, Animated, Easing } from 'react-native';

export default function LiveDriveScreen() {
  const [riskScore, setRiskScore] = useState(0);
  const [driving, setDriving] = useState(false);
  const [speed, setSpeed] = useState(0);
  const [hrv, setHrv] = useState(0);
  const [perclos, setPerclos] = useState(0);
  const fadeAnim = useState(new Animated.Value(0))[0];

  useEffect(() => {
    // In production, connect to WebSocket for real-time updates
    Animated.loop(
      Animated.sequence([
        Animated.timing(fadeAnim, {
          toValue: 1, duration: 1000, easing: Easing.ease, useNativeDriver: true,
        }),
        Animated.timing(fadeAnim, {
          toValue: 0.5, duration: 1000, easing: Easing.ease, useNativeDriver: true,
        }),
      ])
    ).start();

    // Simulated data for demo
    const interval = setInterval(() => {
      setRiskScore(Math.floor(Math.random() * 40));
      setSpeed(45 + Math.floor(Math.random() * 30));
      setHrv(35 + Math.floor(Math.random() * 20));
      setPerclos(Math.random() * 0.15);
      setDriving(true);
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  const riskColor = riskScore < 30 ? '#4CAF50' :
                     riskScore < 50 ? '#FF9800' :
                     riskScore < 70 ? '#FF5722' : '#F44336';

  const riskLabel = riskScore < 30 ? 'Alert' :
                     riskScore < 50 ? 'Low Risk' :
                     riskScore < 70 ? 'Moderate' :
                     riskScore < 85 ? 'High Risk' : 'Critical';

  return (
    <View style={styles.container}>
      <Text style={styles.title}>DriveSync Live</Text>

      {/* Risk Gauge */}
      <View style={[styles.gauge, { borderColor: riskColor }]}>
        <Animated.View
          style={[styles.gaugeFill, {
            height: `${riskScore}%`,
            backgroundColor: riskColor,
            opacity: riskScore > 70 ? fadeAnim : 1,
          }]}
        />
        <Text style={styles.gaugeValue}>{riskScore}</Text>
        <Text style={[styles.gaugeLabel, { color: riskColor }]}>
          {riskLabel}
        </Text>
      </View>

      {/* Metrics */}
      <View style={styles.metricsRow}>
        <View style={styles.metricCard}>
          <Text style={styles.metricValue}>{speed}</Text>
          <Text style={styles.metricLabel}>km/h</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricValue}>{hrv}</Text>
          <Text style={styles.metricLabel}>HRV (ms)</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricValue}>{(perclos * 100).toFixed(1)}%</Text>
          <Text style={styles.metricLabel}>PERCLOS</Text>
        </View>
      </View>

      {riskScore > 70 && (
        <View style={styles.alertBanner}>
          <Text style={styles.alertText}>⚠ DROWSINESS ALERT — PULL OVER SAFELY</Text>
        </View>
      )}

      <Text style={styles.statusText}>
        {driving ? '🟢 Driving — Monitoring Active' : '⚪ Vehicle Parked'}
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e', alignItems: 'center', paddingTop: 40 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 20 },
  gauge: {
    width: 200, height: 200, borderRadius: 100, borderWidth: 4,
    justifyContent: 'flex-end', overflow: 'hidden', marginBottom: 30,
    backgroundColor: '#16213e',
  },
  gaugeFill: { width: '100%', position: 'absolute', bottom: 0 },
  gaugeValue: {
    fontSize: 48, fontWeight: 'bold', color: '#fff',
    position: 'absolute', top: 60, alignSelf: 'center',
  },
  gaugeLabel: {
    fontSize: 16, fontWeight: '600',
    position: 'absolute', top: 115, alignSelf: 'center',
  },
  metricsRow: { flexDirection: 'row', gap: 12, marginBottom: 20 },
  metricCard: {
    backgroundColor: '#16213e', borderRadius: 12, padding: 16,
    alignItems: 'center', minWidth: 90,
  },
  metricValue: { fontSize: 24, fontWeight: 'bold', color: '#fff' },
  metricLabel: { fontSize: 12, color: '#888', marginTop: 4 },
  alertBanner: {
    backgroundColor: '#F44336', borderRadius: 8, padding: 12,
    marginTop: 16,
  },
  alertText: { color: '#fff', fontWeight: 'bold', fontSize: 16 },
  statusText: { color: '#888', marginTop: 20, fontSize: 14 },
});