import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function SafetyForecastScreen() {
  const forecast = [
    { time: '07:00', risk: 55, level: 'moderate' },
    { time: '10:00', risk: 25, level: 'low' },
    { time: '13:00', risk: 20, level: 'low' },
    { time: '17:00', risk: 65, level: 'high' },
    { time: '20:00', risk: 35, level: 'moderate' },
  ];
  const colors: Record<string, string> = {
    low: '#2ecc71', moderate: '#f39c12', high: '#e74c3c',
  };
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>48-Hour Crash Risk</Text>
      <Text style={styles.subtitle}>Best riding window: 10:00 - 14:00</Text>
      {forecast.map((f, i) => (
        <View key={i} style={styles.forecastRow}>
          <Text style={styles.forecastTime}>{f.time}</Text>
          <View style={[styles.riskBar, { backgroundColor: colors[f.level] }]}>
            <Text style={styles.riskText}>{f.risk}% {f.level}</Text>
          </View>
        </View>
      ))}
    </ScrollView>
  );
}
const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 4 },
  subtitle: { fontSize: 14, color: '#7f8c8d', marginBottom: 20 },
  forecastRow: { flexDirection: 'row', alignItems: 'center', marginBottom: 10 },
  forecastTime: { fontSize: 16, color: '#fff', width: 60 },
  riskBar: { flex: 1, padding: 10, borderRadius: 8, marginLeft: 10 },
  riskText: { fontSize: 14, color: '#fff', fontWeight: 'bold' },
});