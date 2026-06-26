/**
 * BrewSync BatchDetailScreen
 * Shows fermentation timeline, SG/temperature curves, predictions
 */
import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function BatchDetailScreen({ route }: any) {
  const { batchId, batchName } = route?.params || {};

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>{batchName || 'Batch Details'}</Text>
      <Text style={styles.subtitle}>Batch ID: {batchId}</Text>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>📈 Fermentation Progress</Text>
        <Text style={styles.placeholder}>[SG curve chart — react-native-chart-kit]</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>🌡 Temperature</Text>
        <Text style={styles.placeholder}>[Temperature chart with target overlay]</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>🫧 CO2 Evolution</Text>
        <Text style={styles.placeholder}>[CO2 ppm over time]</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>🔮 Predictions</Text>
        <Text style={styles.info}>Estimated FG: 1.012</Text>
        <Text style={styles.info}>Completion: ~3 days</Text>
        <Text style={styles.info}>Stuck probability: 3%</Text>
        <Text style={styles.info}>Infection risk: 1%</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>📊 pH & Pressure</Text>
        <Text style={styles.placeholder}>[pH and pressure charts]</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff' },
  subtitle: { fontSize: 14, color: '#8b949e', marginBottom: 16 },
  card: { backgroundColor: '#161b22', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardTitle: { fontSize: 16, fontWeight: '600', color: '#e6e6e6', marginBottom: 8 },
  placeholder: { color: '#8b949e', fontSize: 14, fontStyle: 'italic' },
  info: { color: '#c9d1d9', fontSize: 14, marginBottom: 4 },
});