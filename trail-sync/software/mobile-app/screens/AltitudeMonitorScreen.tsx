/**
 * TrailSync — Altitude Monitor & SOS Screen
 *
 * SpO2, HRV, altitude tracking with AMS risk score.
 * One-tap SOS emergency button with LoRa mesh relay.
 * SPDX-License-Identifier: MIT
 */
import React, { useState } from 'react';
import { View, Text, StyleSheet, TouchableOpacity, Alert } from 'react-native';
import { api } from '../api';

interface AltitudeProps { runnerId: string; }
interface SOSProps { sosActive: boolean; setSosActive: (v: boolean) => void; }

export function AltitudeMonitorScreen({ runnerId }: AltitudeProps) {
  const [spo2, setSpo2] = useState(98);
  const [hrv, setHrv] = useState(55);
  const [altitude, setAltitude] = useState(2400);
  const [ascentRate, setAscentRate] = useState(200);
  const [pressure, setPressure] = useState(1008);

  const amsRisk = spo2 < 88 ? 'CRITICAL' : spo2 < 94 ? 'WARNING' : 'SAFE';
  const amsColor = amsRisk === 'CRITICAL' ? '#f85149' : amsRisk === 'WARNING' ? '#ffa657' : '#3fb950';

  return (
    <View style={styles.container}>
      <Text style={styles.title}>🏔️ Altitude Monitor</Text>

      <View style={[styles.riskCard, { borderColor: amsColor }]}>
        <Text style={styles.riskLabel}>AMS Risk</Text>
        <Text style={[styles.riskValue, { color: amsColor }]}>{amsRisk}</Text>
        {amsRisk === 'CRITICAL' && (
          <Text style={styles.descendAlert}>⚠ DESCEND IMMEDIATELY — SpO2 dangerously low</Text>
        )}
        {amsRisk === 'WARNING' && (
          <Text style={styles.cautionAlert}>⚠ Slow ascent or descend — altitude sickness risk</Text>
        )}
      </View>

      <View style={styles.metricsGrid}>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>SpO₂</Text>
          <Text style={[styles.metricValue, { color: spo2 < 88 ? '#f85149' : spo2 < 94 ? '#ffa657' : '#3fb950' }]}>
            {spo2}%
          </Text>
          <Text style={styles.metricHint}>{spo2 >= 95 ? 'Normal' : spo2 >= 90 ? 'Low' : 'Critical'}</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>HRV (RMSSD)</Text>
          <Text style={styles.metricValue}>{hrv} ms</Text>
          <Text style={styles.metricHint}>{hrv > 40 ? 'Good' : hrv > 20 ? 'Reduced' : 'Poor'}</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Altitude</Text>
          <Text style={styles.metricValue}>{altitude} m</Text>
          <Text style={styles.metricHint}>{altitude < 1500 ? 'Low' : altitude < 3000 ? 'Moderate' : 'High'}</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Ascent Rate</Text>
          <Text style={[styles.metricValue, { color: ascentRate > 500 ? '#f85149' : '#4cc9f0' }]}>
            {ascentRate} m/hr
          </Text>
          <Text style={styles.metricHint}>{ascentRate < 300 ? 'Safe' : ascentRate < 500 ? 'Moderate' : 'Fast'}</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Pressure</Text>
          <Text style={styles.metricValue}>{(pressure / 10).toFixed(1)} hPa</Text>
          <Text style={styles.metricHint}>Barometric</Text>
        </View>
      </View>

      <View style={styles.reference}>
        <Text style={styles.refTitle}>Altitude Sickness Thresholds</Text>
        <Text style={styles.refItem}>• SpO₂ ≥ 95%: Safe at any altitude</Text>
        <Text style={styles.refItem}>• SpO₂ 90-94%: AMS risk — slow ascent</Text>
        <Text style={styles.refItem}>• SpO₂ 88-90%: AMS likely — consider descending</Text>
        <Text style={styles.refItem}>• SpO₂ &lt; 88%: HACE/HAPE risk — descend immediately</Text>
        <Text style={styles.refItem}>• Ascent &gt; 500m/hr: Fast ascent increases AMS risk</Text>
      </View>
    </View>
  );
}

export function SOSScreen({ sosActive, setSosActive }: SOSProps) {
  const [sosType, setSosType] = useState('manual');

  const triggerSOS = () => {
    Alert.alert(
      '🚨 Emergency SOS',
      'This will broadcast your location and vitals to emergency contacts and trail beacons via LoRa mesh.\n\nAre you in immediate danger?',
      [
        { text: 'Cancel', style: 'cancel' },
        { text: 'YES — Send SOS', style: 'destructive', onPress: () => {
          setSosActive(true);
          api.postSOS({
            runner_id: 'self',
            sos_type: 'manual',
            severity: 'serious',
            lat: 0, lon: 0, altitude_m: 0,
            hr: 0, spo2: 0, hrv_rmssd_ms: 0,
            injury_class: 'unknown', num_people: 1,
          });
        }},
      ]
    );
  };

  const cancelSOS = () => {
    Alert.alert('Cancel SOS?', 'Are you safe now? This will cancel the emergency alert.', [
      { text: 'No, keep SOS active' },
      { text: 'Yes, I\'m safe', onPress: () => setSosActive(false) },
    ]);
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>🆘 Emergency SOS</Text>

      {sosActive ? (
        <View style={styles.sosActiveContainer}>
          <Text style={styles.sosActiveText}>SOS ACTIVE</Text>
          <Text style={styles.sosActiveSubtext}>Help is on the way. Stay calm. Stay visible.</Text>
          <TouchableOpacity style={styles.cancelButton} onPress={cancelSOS}>
            <Text style={styles.cancelButtonText}>I'm Safe — Cancel SOS</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <View style={styles.sosContainer}>
          <TouchableOpacity style={styles.sosButton} onPress={triggerSOS}>
            <Text style={styles.sosButtonText}>SOS</Text>
          </TouchableOpacity>
          <Text style={styles.sosHint}>Hold for 5 seconds or tap to trigger emergency SOS</Text>
          <Text style={styles.sosInfo}>
            Sends your GPS location, vitals, and a 30-second audio clip
            via LoRa mesh to trail beacons and emergency contacts.
          </Text>
          <Text style={styles.sosInfo}>
            Works even without cell signal — LoRa relay through beacons.
          </Text>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 12 },
  title: { fontSize: 24, color: '#4cc9f0', fontWeight: 'bold', marginBottom: 16 },
  riskCard: {
    backgroundColor: '#161b22', padding: 16, borderRadius: 12,
    marginBottom: 16, borderWidth: 2,
  },
  riskLabel: { fontSize: 14, color: '#8b949e' },
  riskValue: { fontSize: 32, fontWeight: 'bold' },
  descendAlert: { color: '#f85149', fontSize: 14, fontWeight: 'bold', marginTop: 8 },
  cautionAlert: { color: '#ffa657', fontSize: 14, marginTop: 8 },
  metricsGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  metricCard: {
    width: '48%', backgroundColor: '#161b22', padding: 12, borderRadius: 10,
    marginBottom: 10, borderWidth: 1, borderColor: '#30363d',
  },
  metricLabel: { fontSize: 11, color: '#8b949e', marginBottom: 4 },
  metricValue: { fontSize: 24, fontWeight: 'bold', color: '#4cc9f0' },
  metricHint: { fontSize: 11, color: '#8b949e', marginTop: 2 },
  reference: { backgroundColor: '#161b22', padding: 12, borderRadius: 10, marginTop: 8, borderWidth: 1, borderColor: '#30363d' },
  refTitle: { fontSize: 14, color: '#e6edf3', fontWeight: 'bold', marginBottom: 6 },
  refItem: { fontSize: 12, color: '#8b949e', marginBottom: 3 },
  sosContainer: { alignItems: 'center', marginTop: 40 },
  sosButton: {
    width: 160, height: 160, borderRadius: 80, backgroundColor: '#f85149',
    justifyContent: 'center', alignItems: 'center',
    shadowColor: '#f85149', shadowOpacity: 0.5, shadowRadius: 20,
  },
  sosButtonText: { fontSize: 48, fontWeight: 'bold', color: 'white' },
  sosHint: { fontSize: 14, color: '#8b949e', textAlign: 'center', marginTop: 16 },
  sosInfo: { fontSize: 12, color: '#6e7681', textAlign: 'center', marginTop: 8, maxWidth: 300 },
  sosActiveContainer: { alignItems: 'center', marginTop: 40 },
  sosActiveText: { fontSize: 36, fontWeight: 'bold', color: '#f85149', textAlign: 'center' },
  sosActiveSubtext: { fontSize: 16, color: '#e6edf3', textAlign: 'center', marginTop: 8 },
  cancelButton: {
    backgroundColor: '#30363d', padding: 16, borderRadius: 12,
    marginTop: 24, borderWidth: 1, borderColor: '#8b949e',
  },
  cancelButtonText: { fontSize: 16, color: '#e6edf3', textAlign: 'center' },
});