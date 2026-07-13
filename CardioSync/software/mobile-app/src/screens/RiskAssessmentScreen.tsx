/**
 * RiskAssessmentScreen — 30-day stroke risk forecast + risk factors
 *
 * License: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ActivityIndicator } from 'react-native';
import { useApi } from '../api/client';

export default function RiskAssessmentScreen() {
  const api = useApi();
  const [risk, setRisk] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchRisk = async () => {
      try {
        const data = await api.getStrokeRisk();
        setRisk(data);
      } catch (e) {
        console.error(e);
      } finally {
        setLoading(false);
      }
    };
    fetchRisk();
  }, []);

  if (loading) return <ActivityIndicator size="large" color="#e74c3c" />;
  if (!risk) return <Text style={styles.error}>Unable to assess risk</Text>;

  const riskColor = risk.stroke_risk_30d > 30 ? '#c0392b' :
                    risk.stroke_risk_30d > 15 ? '#e74c3c' :
                    risk.stroke_risk_30d > 5 ? '#f39c12' : '#27ae60';
  const riskLabel = risk.stroke_risk_30d > 30 ? 'High Risk' :
                    risk.stroke_risk_30d > 15 ? 'Elevated Risk' :
                    risk.stroke_risk_30d > 5 ? 'Moderate Risk' : 'Low Risk';

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Stroke Risk Assessment</Text>
      <Text style={styles.subtitle}>30-day forecast</Text>

      <View style={[styles.gaugeCard, { borderColor: riskColor }]}>
        <Text style={[styles.gaugeValue, { color: riskColor }]}>
          {risk.stroke_risk_30d.toFixed(1)}%
        </Text>
        <Text style={[styles.gaugeLabel, { color: riskColor }]}>{riskLabel}</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Risk Factors</Text>
        <Text style={styles.factor}>AFib Events (24h): {risk.risk_factors.afib_events_24h}</Text>
        <Text style={styles.factor}>AFib Burden: {risk.afib_burden_pct.toFixed(1)}%</Text>
        <Text style={styles.factor}>Latest BP: {risk.risk_factors.latest_bp || 'N/A'}</Text>
        <Text style={styles.factor}>Latest RMSSD: {risk.risk_factors.latest_rmssd || 'N/A'} ms</Text>
        <Text style={styles.factor}>CHA₂DS₂-VASc: {risk.risk_factors.chads_vasc_score}</Text>
      </View>

      <Text style={styles.disclaimer}>
        ⚠️ This assessment is not a medical diagnosis. Consult your cardiologist.
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#2c3e50', padding: 20 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff' },
  subtitle: { fontSize: 14, color: '#95a5a6', marginBottom: 20 },
  gaugeCard: {
    backgroundColor: '#34495e', borderRadius: 16, padding: 30,
    alignItems: 'center', marginBottom: 20, borderWidth: 3,
  },
  gaugeValue: { fontSize: 56, fontWeight: 'bold' },
  gaugeLabel: { fontSize: 18, fontWeight: 'bold', marginTop: 5 },
  card: { backgroundColor: '#34495e', borderRadius: 12, padding: 20, marginBottom: 20 },
  cardTitle: { fontSize: 18, fontWeight: 'bold', color: '#fff', marginBottom: 10 },
  factor: { fontSize: 14, color: '#bdc3c7', marginBottom: 5 },
  disclaimer: { fontSize: 12, color: '#e67e22', textAlign: 'center' },
  error: { color: '#e74c3c', fontSize: 16, textAlign: 'center', marginTop: 50 },
});