import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function FallRiskScreen() {
  const risk = 45;
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Fall Risk</Text>
      <View style={[styles.riskCard, { backgroundColor: risk > 60 ? '#e74c3c' : risk > 40 ? '#f39c12' : '#2ecc71' }]}>
        <Text style={styles.riskScore}>{risk}/100</Text>
        <Text style={styles.riskLabel}>30-Day Fall Risk</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Contributing Factors</Text>
        <Text style={styles.metric}>Stride variability (CV): 12.3% (elevated)</Text>
        <Text style={styles.metric}>FOG frequency: 3.2/day</Text>
        <Text style={styles.metric}>Festination events: 1.4/day</Text>
        <Text style={styles.metric}>Near-fall events (7d): 2</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Recommendations</Text>
        <Text style={styles.metric}>- Continue gait training exercises</Text>
        <Text style={styles.metric}>- Use assistive device in unfamiliar settings</Text>
        <Text style={styles.metric}>- PT referral recommended</Text>
        <Text style={styles.metric}>- Review medication timing with neurologist</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50', marginBottom: 12 },
  riskCard: { padding: 30, borderRadius: 12, alignItems: 'center', marginBottom: 12 },
  riskScore: { fontSize: 48, fontWeight: 'bold', color: '#fff' },
  riskLabel: { fontSize: 16, color: '#fff', opacity: 0.9 },
  card: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginBottom: 10 },
  cardTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#2c3e50' },
  metric: { fontSize: 14, color: '#34495e', marginBottom: 4 },
});