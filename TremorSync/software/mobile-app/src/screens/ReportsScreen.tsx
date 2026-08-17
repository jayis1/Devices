import React from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Linking } from 'react-native';

export default function ReportsScreen() {
  const generateReport = (type: string) => {
    Linking.openURL(`https://api.tremorsync.cloud/api/v1/reports/${type}`);
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Reports</Text>
      <TouchableOpacity style={styles.reportCard} onPress={() => generateReport('daily')}>
        <Text style={styles.reportTitle}>Daily PD Summary</Text>
        <Text style={styles.reportDesc}>ON/OFF timeline, tremor summary, dose log</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.reportCard} onPress={() => generateReport('weekly')}>
        <Text style={styles.reportTitle}>Weekly Trend Report</Text>
        <Text style={styles.reportDesc}>7-day trends for tremor, gait, voice, medication</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.reportCard} onPress={() => generateReport('clinical')}>
        <Text style={styles.reportTitle}>Clinical Report (MDS-UPDRS)</Text>
        <Text style={styles.reportDesc}>Neurologist-ready PDF with motor scores, progression</Text>
      </TouchableOpacity>
      <View style={styles.note}>
        <Text style={styles.noteText}>All reports are HIPAA-compliant.</Text>
        <Text style={styles.noteText}>Clinical reports use de-identified data format.</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50', marginBottom: 12 },
  reportCard: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  reportTitle: { fontSize: 18, fontWeight: 'bold', color: '#2c3e50', marginBottom: 4 },
  reportDesc: { fontSize: 14, color: '#7f8c8d' },
  note: { padding: 16, marginTop: 10 },
  noteText: { fontSize: 12, color: '#95a5a6', marginBottom: 4 },
});