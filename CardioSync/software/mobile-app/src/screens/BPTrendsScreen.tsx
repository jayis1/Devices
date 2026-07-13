/**
 * BPTrendsScreen — Blood pressure trends + hypertension staging
 *
 * License: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ActivityIndicator, Picker } from 'react-native';
import { useApi } from '../api/client';

export default function BPTrendsScreen() {
  const api = useApi();
  const [trends, setTrends] = useState(null);
  const [days, setDays] = useState(7);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchTrends = async () => {
      setLoading(true);
      try {
        const data = await api.getBPTrends(days);
        setTrends(data);
      } catch (e) {
        console.error(e);
      } finally {
        setLoading(false);
      }
    };
    fetchTrends();
  }, [days]);

  if (loading) return <ActivityIndicator size="large" color="#e74c3c" />;
  if (!trends || trends.count === 0) {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>Blood Pressure</Text>
        <Text style={styles.subtitle}>No BP data yet. Measurements are taken automatically.</Text>
      </View>
    );
  }

  const categoryColors = {
    'Optimal': '#27ae60',
    'Normal': '#2ecc71',
    'High Normal': '#f1c40f',
    'Hypertension Stage 1': '#e67e22',
    'Hypertension Stage 2': '#e74c3c',
    'Hypertension Stage 3 (Severe)': '#c0392b',
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Blood Pressure</Text>
      <Picker
        selectedValue={days}
        style={styles.picker}
        onValueChange={setDays}
      >
        <Picker.Item label="7 days" value={7} />
        <Picker.Item label="30 days" value={30} />
        <Picker.Item label="90 days" value={90} />
      </Picker>

      <View style={[styles.card, { borderColor: categoryColors[trends.latest_category] || '#34495e' }]}>
        <Text style={styles.cardLabel}>Latest BP</Text>
        <Text style={styles.cardValue}>{trends.latest_systolic}/{trends.latest_diastolic} mmHg</Text>
        <Text style={[styles.category, { color: categoryColors[trends.latest_category] }]}>
          {trends.latest_category}
        </Text>
      </View>

      <View style={styles.row}>
        <View style={styles.cardSmall}>
          <Text style={styles.cardLabel}>Avg Systolic</Text>
          <Text style={styles.cardValue}>{trends.avg_systolic.toFixed(0)}</Text>
        </View>
        <View style={styles.cardSmall}>
          <Text style={styles.cardLabel}>Avg Diastolic</Text>
          <Text style={styles.cardValue}>{trends.avg_diastolic.toFixed(0)}</Text>
        </View>
      </View>

      <View style={styles.row}>
        <View style={styles.cardSmall}>
          <Text style={styles.cardLabel}>Trend (sys)</Text>
          <Text style={styles.cardValue}>
            {trends.sys_trend_slope > 0 ? '↑' : trends.sys_trend_slope < 0 ? '↓' : '→'}
            {Math.abs(trends.sys_trend_slope).toFixed(1)} mmHg/day
          </Text>
        </View>
        <View style={styles.cardSmall}>
          <Text style={styles.cardLabel}>Readings</Text>
          <Text style={styles.cardValue}>{trends.count}</Text>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#2c3e50', padding: 20 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 10 },
  subtitle: { fontSize: 14, color: '#95a5a6', marginTop: 10 },
  picker: { backgroundColor: '#34495e', color: '#fff', marginBottom: 15 },
  card: {
    backgroundColor: '#34495e', borderRadius: 12, padding: 20,
    marginBottom: 12, borderWidth: 2,
  },
  cardSmall: {
    backgroundColor: '#34495e', borderRadius: 12, padding: 15,
    flex: 1, marginHorizontal: 6, marginBottom: 12, borderWidth: 2,
    borderColor: '#34495e',
  },
  row: { flexDirection: 'row' },
  cardLabel: { fontSize: 12, color: '#95a5a6', marginBottom: 5 },
  cardValue: { fontSize: 24, fontWeight: 'bold', color: '#fff' },
  category: { fontSize: 16, fontWeight: 'bold', marginTop: 5 },
});