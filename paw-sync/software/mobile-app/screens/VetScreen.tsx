// VetScreen — Vet report generation + share

import React, { useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Alert, Share } from 'react-native';
import { getVetReport, VetReport } from '../api';

const PET_ID = 1;

export default function VetScreen() {
  const [report, setReport] = useState<VetReport | null>(null);
  const [loading, setLoading] = useState(false);

  const generateReport = async () => {
    setLoading(true);
    try {
      const data = await getVetReport(PET_ID);
      setReport(data);
    } catch (e) {
      Alert.alert('Error', 'Failed to generate vet report');
      console.error(e);
    }
    setLoading(false);
  };

  const shareReport = async () => {
    if (!report) return;
    const text = `PawSync Vet Report — Pet #${report.pet_id}
Generated: ${report.generated_at}

BASELINE: ${report.baseline.established ? 'Established' : 'Learning'}
  HR: ${report.baseline.baseline_hr.toFixed(0)} bpm
  HRV: ${report.baseline.baseline_hrv_ms.toFixed(1)} ms

CURRENT VITALS:
  HR: ${report.current_vitals.hr} bpm
  HRV: ${report.current_vitals.hrv_ms.toFixed(1)} ms
  Temp: ${report.current_vitals.temp_c.toFixed(1)}°C

WELLNESS: ${report.wellness.wellness}/100
ILLNESS RISK: ${report.wellness.illness_risk}%
ANXIETY: ${report.wellness.anxiety_level}%

RECENT ALERTS: ${report.recent_alerts.length}
APPETITE LOSS EVENTS: ${report.feeding_summary.appetite_loss_count}
ANXIETY EPISODES: ${report.anxiety_episodes.length}`;

    try {
      await Share.share({ message: text, title: 'PawSync Vet Report' });
    } catch (e) { console.error(e); }
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>🏥 Vet Portal</Text>

      <TouchableOpacity style={styles.button} onPress={generateReport} disabled={loading}>
        <Text style={styles.buttonText}>
          {loading ? 'Generating...' : '📋 Generate Vet Report'}
        </Text>
      </TouchableOpacity>

      {report && (
        <>
          <View style={styles.reportCard}>
            <Text style={styles.reportTitle}>Report Summary</Text>
            <Text style={styles.reportDate}>
              Generated: {new Date(report.generated_at).toLocaleString()}
            </Text>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Baseline Status</Text>
              <Text style={styles.sectionText}>
                {report.baseline.established ? '✅ Established' : '⏳ Learning (14-day period)'}
              </Text>
              <Text style={styles.sectionText}>
                Baseline HR: {report.baseline.baseline_hr.toFixed(0)} bpm
              </Text>
              <Text style={styles.sectionText}>
                Baseline HRV: {report.baseline.baseline_hrv_ms.toFixed(1)} ms
              </Text>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Current Vitals</Text>
              <Text style={styles.sectionText}>HR: {report.current_vitals.hr} bpm</Text>
              <Text style={styles.sectionText}>HRV: {report.current_vitals.hrv_ms.toFixed(1)} ms</Text>
              <Text style={styles.sectionText}>Temp: {report.current_vitals.temp_c.toFixed(1)}°C</Text>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Wellness Score</Text>
              <Text style={[styles.wellnessScore, {
                color: report.wellness.wellness >= 70 ? '#4CAF50' :
                       report.wellness.wellness >= 50 ? '#FF9800' : '#F44336'
              }]}>
                {report.wellness.wellness}/100
              </Text>
              <Text style={styles.sectionText}>
                Illness Risk: {report.wellness.illness_risk}%
              </Text>
              <Text style={styles.sectionText}>
                Anxiety Level: {report.wellness.anxiety_level}%
              </Text>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Feeding Summary</Text>
              <Text style={styles.sectionText}>
                Appetite loss events: {report.feeding_summary.appetite_loss_count}
              </Text>
              <Text style={styles.sectionText}>
                Recent meals: {report.feeding_summary.recent_meals.length}
              </Text>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Alerts ({report.recent_alerts.length})</Text>
              {report.recent_alerts.slice(0, 5).map((a, i) => (
                <Text key={i} style={styles.alertText}>
                  [{a.severity}] {a.type}: {a.message}
                </Text>
              ))}
            </View>
          </View>

          <TouchableOpacity style={styles.shareButton} onPress={shareReport}>
            <Text style={styles.buttonText}>📤 Share with Vet</Text>
          </TouchableOpacity>
        </>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 22, fontWeight: 'bold', textAlign: 'center', marginVertical: 16 },
  button: { backgroundColor: '#2196F3', marginHorizontal: 16, padding: 16, borderRadius: 12, alignItems: 'center', marginBottom: 16 },
  shareButton: { backgroundColor: '#4CAF50', marginHorizontal: 16, padding: 16, borderRadius: 12, alignItems: 'center', marginBottom: 32 },
  buttonText: { color: 'white', fontSize: 16, fontWeight: '600' },
  reportCard: { backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  reportTitle: { fontSize: 18, fontWeight: 'bold', marginBottom: 4 },
  reportDate: { fontSize: 11, color: '#999', marginBottom: 16 },
  section: { marginTop: 12, paddingBottom: 12, borderBottomWidth: 1, borderBottomColor: '#eee' },
  sectionTitle: { fontSize: 14, fontWeight: '600', color: '#333', marginBottom: 4 },
  sectionText: { fontSize: 13, color: '#666', lineHeight: 20 },
  wellnessScore: { fontSize: 36, fontWeight: 'bold', marginVertical: 4 },
  alertText: { fontSize: 12, color: '#666', lineHeight: 18 },
});