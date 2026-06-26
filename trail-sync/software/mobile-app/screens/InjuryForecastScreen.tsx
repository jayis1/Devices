/**
 * TrailSync — Injury Forecast Screen
 *
 * 7-day injury risk forecast per body part,
 * training load, and personalized recommendations.
 * SPDX-License-Identifier: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { api } from '../api';

interface Props { runnerId: string; }

const INJURY_COLORS: Record<string, string> = {
  'IT band syndrome': '#f0883e',
  'plantar fasciitis': '#f85149',
  'Achilles tendinopathy': '#da3633',
  'stress fracture': '#ff7b72',
  'shin splints': '#ffa657',
  'runner\'s knee': '#d29922',
  'ankle sprain': '#e3b341',
  'hamstring strain': '#3fb950',
  'hip flexor strain': '#58a6ff',
  'calf strain': '#4cc9f0',
  'IT band friction': '#bc8cff',
  'patellar tendinopathy': '#79c0ff',
};

export function InjuryForecastScreen({ runnerId }: Props) {
  const [forecast, setForecast] = useState<any>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetch = async () => {
      try {
        const data = await api.getInjuryRisk(runnerId);
        setForecast(data);
      } catch (e) { /* offline */ }
      setLoading(false);
    };
    fetch();
    const interval = setInterval(fetch, 60000);
    return () => clearInterval(interval);
  }, [runnerId]);

  if (loading || !forecast) {
    return (
      <View style={styles.container}>
        <Text style={styles.loading}>Loading injury forecast...</Text>
      </View>
    );
  }

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>🦴 Injury Risk Forecast</Text>
      <Text style={styles.subtitle}>7-day risk for {forecast.date}</Text>

      <View style={styles.loadCard}>
        <Text style={styles.loadLabel}>Training Load</Text>
        <Text style={styles.loadValue}>{forecast.training_load}</Text>
        <Text style={styles.loadLabel}>Acute:Chronic Ratio</Text>
        <Text style={[styles.loadValue,
          { color: forecast.acute_chronic_ratio > 1.3 ? '#f85149' : '#3fb950' }
        ]}>
          {forecast.acute_chronic_ratio}
        </Text>
      </View>

      <Text style={styles.sectionTitle}>Risk by Injury</Text>
      {Object.entries(forecast.injuries).map(([injury, risk]: [string, any]) => {
        const riskNum = typeof risk === 'number' ? risk : 0;
        const barWidth = Math.min(riskNum, 100);
        const color = INJURY_COLORS[injury] || '#4cc9f0';
        const riskLevel = riskNum > 60 ? 'HIGH' : riskNum > 30 ? 'MODERATE' : 'LOW';
        return (
          <View key={injury} style={styles.injuryRow}>
            <Text style={styles.injuryName}>{injury}</Text>
            <View style={styles.barContainer}>
              <View style={[styles.bar, { width: `${barWidth}%`, backgroundColor: color }]} />
            </View>
            <Text style={[styles.riskPct, { color }]}>{riskNum.toFixed(0)}%</Text>
            <Text style={[styles.riskLevel, {
              color: riskLevel === 'HIGH' ? '#f85149' : riskLevel === 'MODERATE' ? '#ffa657' : '#3fb950'
            }]}>{riskLevel}</Text>
          </View>
        );
      })}

      <Text style={styles.sectionTitle}>Recommendations</Text>
      {forecast.recommendations.map((rec: string, i: number) => (
        <View key={i} style={styles.recCard}>
          <Text style={styles.recText}>• {rec}</Text>
        </View>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 12 },
  title: { fontSize: 24, color: '#4cc9f0', fontWeight: 'bold' },
  subtitle: { fontSize: 14, color: '#8b949e', marginBottom: 16 },
  loading: { fontSize: 16, color: '#8b949e', textAlign: 'center', marginTop: 40 },
  loadCard: {
    backgroundColor: '#161b22', padding: 16, borderRadius: 12,
    marginBottom: 16, borderWidth: 1, borderColor: '#30363d',
  },
  loadLabel: { fontSize: 12, color: '#8b949e' },
  loadValue: { fontSize: 28, color: '#4cc9f0', fontWeight: 'bold', marginBottom: 8 },
  sectionTitle: { fontSize: 16, color: '#e6edf3', fontWeight: 'bold', marginTop: 16, marginBottom: 8 },
  injuryRow: {
    flexDirection: 'row', alignItems: 'center', marginBottom: 8,
    backgroundColor: '#161b22', padding: 10, borderRadius: 8,
  },
  injuryName: { fontSize: 12, color: '#e6edf3', width: 110 },
  barContainer: { flex: 1, height: 8, backgroundColor: '#30363d', borderRadius: 4, marginHorizontal: 8 },
  bar: { height: 8, borderRadius: 4 },
  riskPct: { fontSize: 12, fontWeight: 'bold', width: 40 },
  riskLevel: { fontSize: 10, fontWeight: 'bold', width: 60 },
  recCard: {
    backgroundColor: '#161b22', padding: 10, borderRadius: 8,
    marginBottom: 6, borderWidth: 1, borderColor: '#30363d',
  },
  recText: { fontSize: 13, color: '#e6edf3' },
});