import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function ProgressionScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Disease Progression</Text>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>MDS-UPDRS Part III (Motor)</Text>
        <Text style={styles.score}>Estimated: 28/132</Text>
        <Text style={styles.metric}>3-month change: +3 points</Text>
        <Text style={styles.metric}>Trend: Slow progression</Text>
        <Text style={styles.metric}>Clinical correlation: r=0.91</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Motor Sub-scores</Text>
        <Text style={styles.metric}>Tremor: 6/20</Text>
        <Text style={styles.metric}>Bradykinesia: 14/40</Text>
        <Text style={styles.metric}>Rigidity: 5/16</Text>
        <Text style={styles.metric}>Gait/Posture: 3/12</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>90-Day Trajectory</Text>
        <Text style={styles.metric}>Tremor severity: Stable</Text>
        <Text style={styles.metric}>Bradykinesia: Slowly worsening</Text>
        <Text style={styles.metric}>Gait: Slowly worsening</Text>
        <Text style={styles.metric}>Speech: Mildly worsening</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50', marginBottom: 12 },
  card: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  cardTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#2c3e50' },
  score: { fontSize: 28, fontWeight: 'bold', color: '#2c3e50', marginBottom: 8 },
  metric: { fontSize: 14, color: '#34495e', marginBottom: 4 },
});