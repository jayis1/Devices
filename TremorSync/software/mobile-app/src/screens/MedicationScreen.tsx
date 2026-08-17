import React from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity } from 'react-native';
import { useTremorSync } from '../services/TremorSyncContext';

export default function MedicationScreen() {
  const { nextDoseMin, onoffState, logManualDose } = useTremorSync();

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Medication</Text>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>ON/OFF Timeline</Text>
        <Text style={styles.metric}>Current: {onoffState === 1 ? 'ON' : onoffState === 0 ? 'OFF' : 'Transition'}</Text>
        <Text style={styles.metric}>Minutes since dose: {240 - nextDoseMin}</Text>
        <Text style={styles.metric}>Predicted OFF in: {nextDoseMin} min</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Dose History (7 days)</Text>
        <Text style={styles.metric}>Doses logged: 26/28</Text>
        <Text style={styles.metric}>Missed: 2 (caregiver alerted)</Text>
        <Text style={styles.metric}>Avg ON time: 68% (target {'>'}75%)</Text>
        <Text style={styles.metric}>OFF episodes: 14</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Pharmacokinetic Profile</Text>
        <Text style={styles.metric}>ON onset: 32 min (avg)</Text>
        <Text style={styles.metric}>ON duration: 198 min (avg)</Text>
        <Text style={styles.metric}>Wearing-off: gradual</Text>
        <Text style={styles.metric}>Protein interaction: Active</Text>
      </View>
      <TouchableOpacity style={styles.button} onPress={logManualDose}>
        <Text style={styles.buttonText}>Log Manual Dose</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50', marginBottom: 12 },
  card: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  cardTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#2c3e50' },
  metric: { fontSize: 14, color: '#34495e', marginBottom: 4 },
  button: { backgroundColor: '#3498db', padding: 16, borderRadius: 10, alignItems: 'center', marginTop: 10 },
  buttonText: { color: '#fff', fontSize: 16, fontWeight: 'bold' },
});