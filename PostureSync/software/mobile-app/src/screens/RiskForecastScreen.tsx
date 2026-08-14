/** Risk Forecast Screen — 90-day spinal health risk + spinal age */
import React, { useState, useEffect, useContext } from 'react';
import { View, Text, StyleSheet, ScrollView, ActivityIndicator } from 'react-native';
import { PostureSyncContext } from '../services/PostureSyncContext';

export default function RiskForecastScreen() {
  const { api } = useContext(PostureSyncContext);
  const [forecast, setForecast] = useState(null);
  const [spinalAge, setSpinalAge] = useState(null);
  const [scoliosis, setScoliosis] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    (async () => {
      try {
        const [f, a, s] = await Promise.all([
          api.getRiskForecast(),
          api.getSpinalAge(),
          api.getScoliosisScreen(),
        ]);
        setForecast(f);
        setSpinalAge(a);
        setScoliosis(s);
      } catch (e) {
        console.error(e);
      } finally {
        setLoading(false);
      }
    })();
  }, []);

  if (loading) return <ActivityIndicator style={styles.loader} size="large" />;

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Health Risk Forecast</Text>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>90-Day Spinal Health Risk</Text>
        <Text style={[styles.riskScore, {
          color: forecast?.risk_score < 30 ? '#4CAF50' : forecast?.risk_score < 60 ? '#FF9800' : '#F44336'
        }]}>
          {forecast?.risk_score || 0}/100
        </Text>
        <Text style={styles.riskLevel}>{forecast?.risk_level?.toUpperCase() || 'LOW'}</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Biological Spinal Age</Text>
        <Text style={styles.ageValue}>{spinalAge?.spinal_age || 35} years</Text>
        <Text style={styles.ageDelta}>
          {spinalAge?.delta > 0 ? '+' : ''}{spinalAge?.delta || 0} vs chronological
        </Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Scoliosis Screening</Text>
        <Text style={styles.scoliosisValue}>{scoliosis?.risk_score || 0}/100</Text>
        <Text style={styles.scoliosisRec}>{scoliosis?.recommendation || 'monitor'}</Text>
      </View>

      {forecast?.factors?.length > 0 && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Risk Factors</Text>
          {forecast.factors.map((f: string, i: number) => (
            <Text key={i} style={styles.factor}>• {f}</Text>
          ))}
        </View>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, backgroundColor: '#FAFAFA' },
  loader: { flex: 1, justifyContent: 'center' },
  title: { fontSize: 24, fontWeight: '700', marginBottom: 16, color: '#212121' },
  card: { padding: 20, backgroundColor: '#FFF', borderRadius: 16, marginBottom: 12, elevation: 2 },
  cardTitle: { fontSize: 14, color: '#757575', marginBottom: 8 },
  riskScore: { fontSize: 48, fontWeight: '800' },
  riskLevel: { fontSize: 16, fontWeight: '600', marginTop: 4 },
  ageValue: { fontSize: 36, fontWeight: '700', color: '#212121' },
  ageDelta: { fontSize: 14, color: '#757575', marginTop: 4 },
  scoliosisValue: { fontSize: 36, fontWeight: '700', color: '#212121' },
  scoliosisRec: { fontSize: 14, color: '#757575', marginTop: 4 },
  factor: { fontSize: 14, color: '#424242', marginBottom: 4 },
});