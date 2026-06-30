/**
 * AsthmaSync — Dashboard Screen
 * Real-time risk gauge, air quality, latest events.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, RefreshControl, ScrollView } from 'react-native';
import { ApiClient } from '../services/api';

interface RiskData {
  risk_score: number;
  risk_level: string;
  confidence: number;
  trend: string;
  contributing_factors: Array<{ factor: string; value: number; weight: number }>;
}

interface AdherenceData {
  rescue_count_7d: number;
  rescue_count_30d: number;
  gina_controlled: boolean;
  last_rescue: string | null;
}

export default function DashboardScreen() {
  const [risk, setRisk] = useState<RiskData | null>(null);
  const [adherence, setAdherence] = useState<AdherenceData | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    const api = ApiClient.getInstance();
    try {
      const [riskData, adherenceData] = await Promise.all([
        api.getRisk(),
        api.getAdherence(),
      ]);
      setRisk(riskData);
      setAdherence(adherenceData);
    } catch (e) {
      console.error('Dashboard fetch error:', e);
    }
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 60000); // refresh every minute
    return () => clearInterval(interval);
  }, [fetchData]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  }, [fetchData]);

  const zoneColor = risk?.risk_level === 'high' ? '#E53935' :
                    risk?.risk_level === 'moderate' ? '#FB8C00' : '#43A047';
  const zoneLabel = risk?.risk_level === 'high' ? 'HIGH RISK' :
                    risk?.risk_level === 'moderate' ? 'MODERATE RISK' : 'WELL CONTROLLED';

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Risk Gauge */}
      <View style={[styles.card, { borderColor: zoneColor }]}>
        <Text style={styles.cardTitle}>7-Day Risk Forecast</Text>
        <View style={styles.gaugeContainer}>
          <Text style={[styles.riskScore, { color: zoneColor }]}>
            {risk ? Math.round(risk.risk_score) : '--'}
          </Text>
          <Text style={styles.riskUnit}>/100</Text>
        </View>
        <Text style={[styles.riskLevel, { color: zoneColor }]}>{zoneLabel}</Text>
        <Text style={styles.trend}>
          Trend: {risk?.trend || '—'}
        </Text>
      </View>

      {/* Contributing Factors */}
      {risk && risk.contributing_factors.length > 0 && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Contributing Factors</Text>
          {risk.contributing_factors.map((f, i) => (
            <View key={i} style={styles.factorRow}>
              <Text style={styles.factorName}>{f.factor.replace(/_/g, ' ')}</Text>
              <Text style={styles.factorValue}>{f.value}</Text>
              <View style={styles.factorBar}>
                <View style={[styles.factorFill, { width: `${f.weight}%`, backgroundColor: zoneColor }]} />
              </View>
            </View>
          ))}
        </View>
      )}

      {/* Medication Summary */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Medication (7 days)</Text>
        <View style={styles.medRow}>
          <Text style={styles.medLabel}>Rescue uses:</Text>
          <Text style={[styles.medValue,
            { color: (adherence?.rescue_count_7d || 0) > 4 ? '#E53935' :
                     (adherence?.rescue_count_7d || 0) > 2 ? '#FB8C00' : '#43A047' }]}>
            {adherence?.rescue_count_7d ?? '--'}
          </Text>
        </View>
        <View style={styles.medRow}>
          <Text style={styles.medLabel}>GINA status:</Text>
          <Text style={[styles.medValue,
            { color: adherence?.gina_controlled ? '#43A047' : '#FB8C00' }]}>
            {adherence?.gina_controlled ? 'Controlled' : 'Partly Controlled'}
          </Text>
        </View>
      </View>

      {/* Quick Tips */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Today's Tips</Text>
        <Text style={styles.tip}>• Check air quality before outdoor activity</Text>
        <Text style={styles.tip}>• Carry rescue inhaler at all times</Text>
        <Text style={styles.tip}>• Take controller medication as prescribed</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f0f4f8' },
  card: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    margin: 12,
    borderWidth: 1,
    borderColor: '#e0e0e0',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  cardTitle: { fontSize: 16, fontWeight: 'bold', color: '#333', marginBottom: 12 },
  gaugeContainer: { flexDirection: 'row', alignItems: 'baseline', justifyContent: 'center' },
  riskScore: { fontSize: 64, fontWeight: 'bold' },
  riskUnit: { fontSize: 20, color: '#999', marginLeft: 4 },
  riskLevel: { fontSize: 18, fontWeight: '600', textAlign: 'center', marginTop: 4 },
  trend: { fontSize: 14, color: '#666', textAlign: 'center', marginTop: 4 },
  factorRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 6 },
  factorName: { flex: 1, fontSize: 13, color: '#555', textTransform: 'capitalize' },
  factorValue: { width: 50, fontSize: 13, fontWeight: '600', textAlign: 'right' },
  factorBar: { flex: 1, height: 6, backgroundColor: '#eee', borderRadius: 3, marginLeft: 8 },
  factorFill: { height: 6, borderRadius: 3 },
  medRow: { flexDirection: 'row', justifyContent: 'space-between', marginVertical: 4 },
  medLabel: { fontSize: 14, color: '#555' },
  medValue: { fontSize: 14, fontWeight: 'bold' },
  tip: { fontSize: 13, color: '#666', marginVertical: 4, lineHeight: 20 },
});