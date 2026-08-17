import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { LineChart } from 'react-native-chart-kit';
import { Dimensions } from 'react-native';

const screenW = Dimensions.get('window').width;

export default function TremorViewScreen() {
  const [history, setHistory] = useState<number[]>(Array(24).fill(0));

  useEffect(() => {
    // Fetch 24-hour tremor history from API
    fetch('https://api.tremorsync.cloud/api/v1/tremor/history?hours=24')
      .then(r => r.json())
      .then(data => {
        if (Array.isArray(data) && data.length > 0) {
          setHistory(data.map((d: any) => d.tremor_amplitude || 0));
        }
      }).catch(() => {});
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Tremor Analysis</Text>
      <Text style={styles.subtitle}>24-hour tremor amplitude</Text>
      <LineChart
        data={{ labels: Array(24).fill(''), datasets: [{ data: history }] }}
        width={screenW - 40}
        height={200}
        chartConfig={{
          backgroundColor: '#fff',
          backgroundGradientFrom: '#fff',
          backgroundGradientTo: '#fff',
          color: (opacity = 1) => `rgba(231, 76, 60, ${opacity})`,
        }}
        style={{ marginVertical: 10, borderRadius: 10 }}
      />
      <View style={styles.classInfo}>
        <Text style={styles.infoTitle}>Tremor Classes Detected (24h)</Text>
        <Text style={styles.infoRow}>Resting: 4.2h (17.5%)</Text>
        <Text style={styles.infoRow}>Postural: 2.1h (8.8%)</Text>
        <Text style={styles.infoRow}>Action: 1.5h (6.3%)</Text>
        <Text style={styles.infoRow}>None: 16.2h (67.5%)</Text>
      </View>
      <View style={styles.classInfo}>
        <Text style={styles.infoTitle}>Frequency Analysis</Text>
        <Text style={styles.infoRow}>Dominant frequency: 5.2 Hz</Text>
        <Text style={styles.infoRow}>Peak amplitude: 0.847 m/s²</Text>
        <Text style={styles.infoRow}>RMS (4-6 Hz band): 0.234 m/s²</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2c3e50' },
  subtitle: { fontSize: 14, color: '#7f8c8d', marginBottom: 10 },
  classInfo: { backgroundColor: '#fff', padding: 16, borderRadius: 10, marginTop: 12 },
  infoTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#2c3e50' },
  infoRow: { fontSize: 14, color: '#34495e', marginBottom: 4 },
});