// TherapistScreen — Therapist report + share

import React, { useState, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Share } from 'react-native';
import { getTherapistReport, TherapistReport } from '../api';

const USER_ID = 1;

export default function TherapistScreen() {
  const [report, setReport] = useState<TherapistReport | null>(null);
  const [loading, setLoading] = useState(false);

  const generateReport = useCallback(async () => {
    setLoading(true);
    try {
      const r = await getTherapistReport(USER_ID);
      setReport(r);
    } catch (e) { console.error(e); }
    setLoading(false);
  }, []);

  const shareReport = async () => {
    if (!report) return;
    const text = `CalmGrid Therapist Report\n` +
      `Generated: ${report.generated_at}\n\n` +
      `Summary:\n` +
      `  Avg HRV (24h): ${report.summary.avg_hrv_24h?.toFixed(1)} ms\n` +
      `  Avg Stress (30d): ${report.summary.avg_stress_30d?.toFixed(0)}/100\n` +
      `  Burnout Risk: ${report.summary.burnout_risk}/100\n` +
      `  Acute Stress Episodes: ${report.summary.acute_stress_episodes}\n\n` +
      `Recent Alerts: ${report.recent_alerts.length}`;
    await Share.share({ message: text });
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Therapist Report</Text>
      <Text style={styles.subtitle}>Objective physiological data for your therapy sessions</Text>

      <TouchableOpacity style={styles.button} onPress={generateReport} disabled={loading}>
        <Text style={styles.buttonText}>{loading ? 'Generating...' : 'Generate Report'}</Text>
      </TouchableOpacity>

      {report && (
        <>
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Summary</Text>
            <Text style={styles.stat}>Avg HRV (24h): {report.summary.avg_hrv_24h?.toFixed(1)} ms</Text>
            <Text style={styles.stat}>Avg Stress (30d): {report.summary.avg_stress_30d?.toFixed(0)}/100</Text>
            <Text style={styles.stat}>Burnout Risk: {report.summary.burnout_risk}/100</Text>
            <Text style={styles.stat}>Acute Stress Episodes: {report.summary.acute_stress_episodes}</Text>
          </View>

          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Recent Alerts ({report.recent_alerts.length})</Text>
            {report.recent_alerts.map((a, i) => (
              <Text key={i} style={styles.alertItem}>• [{a.severity}] {a.type}: {a.message}</Text>
            ))}
          </View>

          <TouchableOpacity style={styles.shareButton} onPress={shareReport}>
            <Text style={styles.shareText}>Share with Therapist</Text>
          </TouchableOpacity>
        </>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  subtitle: { fontSize: 14, color: '#666', textAlign: 'center', marginBottom: 20 },
  button: { backgroundColor: '#6C63FF', padding: 16, borderRadius: 12, marginHorizontal: 16, alignItems: 'center' },
  buttonText: { color: 'white', fontSize: 16, fontWeight: 'bold' },
  section: { backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 12 },
  stat: { fontSize: 14, color: '#333', marginBottom: 8 },
  alertItem: { fontSize: 13, color: '#555', marginBottom: 4 },
  shareButton: { backgroundColor: '#4CAF50', padding: 16, borderRadius: 12, margin: 16, alignItems: 'center' },
  shareText: { color: 'white', fontSize: 16, fontWeight: 'bold' },
});