import React from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity } from 'react-native';

export default function CaregiverScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Caregiver View</Text>
      <View style={[styles.card, { borderLeftColor: '#2ecc71', borderLeftWidth: 4 }]}>
        <Text style={styles.cardTitle}>Patient Status</Text>
        <Text style={styles.metric}>State: ON</Text>
        <Text style={styles.metric}>Last dose: 1h 22m ago</Text>
        <Text style={styles.metric}>Next dose in: 2h 38m</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Today's Alerts</Text>
        <Text style={styles.metric}>- 2 FOG episodes (managed with cueing)</Text>
        <Text style={styles.metric}>- No falls detected</Text>
        <Text style={styles.metric}>- No missed doses</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>7-Day Summary</Text>
        <Text style={styles.metric}>ON time: 68% (target {'>'}75%)</Text>
        <Text style={styles.metric}>Missed doses: 2</Text>
        <Text style={styles.metric}>Fall risk: Moderate (45/100)</Text>
        <Text style={styles.metric}>Speech: Slight decline noted</Text>
      </View>
      <TouchableOpacity style={styles.button}>
        <Text style={styles.buttonText}>Contact Neurologist</Text>
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