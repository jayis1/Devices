import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function VoiceViewScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Voice & Swallow</Text>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Speech Quality</Text>
        <Text style={styles.metric}>Class: Mild Hypophonia</Text>
        <Text style={styles.metric}>Hypophonia Score: 32/100</Text>
        <Text style={styles.metric}>F0 Mean: 112 Hz</Text>
        <Text style={styles.metric}>F0 Std: 18 Hz (reduced prosody)</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>7-Day Speech Trend</Text>
        <Text style={styles.metric}>Hypophonia: Worsening (+8%)</Text>
        <Text style={styles.metric}>Recommendation: Speech therapy referral</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Swallow Safety</Text>
        <Text style={styles.metric}>Swallows today: 42</Text>
        <Text style={styles.metric}>Prolonged swallows: 3 (7%)</Text>
        <Text style={styles.metric}>Cough-after-swallow: 1 (aspiration sign)</Text>
        <Text style={styles.warning}>Warning: Elevated aspiration risk</Text>
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
  warning: { fontSize: 14, color: '#e74c3c', fontWeight: 'bold', marginTop: 6 },
});