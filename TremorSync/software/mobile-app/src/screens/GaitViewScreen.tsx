import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function GaitViewScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Gait Analysis</Text>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Current Gait Metrics</Text>
        <Text style={styles.metric}>Stride Length: 0.62 m</Text>
        <Text style={styles.metric}>Cadence: 98 steps/min</Text>
        <Text style={styles.metric}>Double Support: 28%</Text>
        <Text style={styles.metric}>Freeze Index: 0.12</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>FOG Episodes (24h)</Text>
        <Text style={styles.metric}>Total: 3 episodes</Text>
        <Text style={styles.metric}>Avg duration: 8.2s</Text>
        <Text style={styles.metric}>Triggers: Doorway (2), Turn (1)</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Festination Events</Text>
        <Text style={styles.metric}>Today: 1 event</Text>
        <Text style={styles.metric}>7-day trend: Decreasing</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Gait Variability</Text>
        <Text style={styles.metric}>Stride CV: 12.3% (elevated)</Text>
        <Text style={styles.metric}>Normal range: 3-8%</Text>
        <Text style={styles.metric}>Fall risk indicator: Moderate</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50', marginBottom: 12 },
  card: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  cardTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#2c3e50' },
  metric: { fontSize: 14, color: '#34495e', marginBottom: 4 },
});