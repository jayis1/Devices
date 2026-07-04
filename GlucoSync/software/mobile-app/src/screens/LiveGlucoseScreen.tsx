/**
 * GlucoSync — Live Glucose Screen
 *
 * Real-time glucose gauge + 30/60-min forecast + trend arrow.
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, Dimensions } from 'react-native';
import { LineChart } from 'react-native-chart-kit';
import { GlucoSyncAPI, GlucoseReading } from '../api/client';
import { useAuth } from '../context/AuthContext';

const screenWidth = Dimensions.get('window').width;

function getTrendArrow(trend: number): string {
  if (trend > 2) return '↑↑';   // rising fast
  if (trend > 1) return '↑';     // rising
  if (trend > -1) return '→';   // stable
  if (trend > -2) return '↓';    // falling
  return '↓↓';                   // falling fast
}

function getGlucoseColor(glucose: number): string {
  if (glucose < 54) return '#DC2626';   // critical low
  if (glucose < 70) return '#F59E0B';   // low
  if (glucose <= 180) return '#10B981'; // in range
  if (glucose <= 250) return '#F59E0B'; // high
  return '#DC2626';                       // critical high
}

export default function LiveGlucoseScreen() {
  const { userId } = useAuth();
  const [readings, setReadings] = useState<GlucoseReading[]>([]);
  const [loading, setLoading] = useState(true);

  const fetchGlucose = useCallback(async () => {
    if (!userId) return;
    try {
      const data = await GlucoSyncAPI.getGlucose(userId, 6); // last 6 hours
      setReadings(data);
    } catch (e) {
      console.error('Failed to fetch glucose:', e);
    } finally {
      setLoading(false);
    }
  }, [userId]);

  useEffect(() => {
    fetchGlucose();
    const interval = setInterval(fetchGlucose, 60000); // refresh every minute
    return () => clearInterval(interval);
  }, [fetchGlucose]);

  const latest = readings[readings.length - 1];
  const chartData = readings.slice(-60).map(r => r.glucose_mgdl);

  return (
    <View style={styles.container}>
      {latest && (
        <View style={styles.gaugeContainer}>
          <Text style={[styles.glucoseValue, { color: getGlucoseColor(latest.glucose_mgdl) }]}>
            {latest.glucose_mgdl}
          </Text>
          <Text style={styles.glucoseUnit}>mg/dL</Text>
          <Text style={styles.trendArrow}>{getTrendArrow(latest.trend)}</Text>

          <View style={styles.forecastRow}>
            <View style={styles.forecastBox}>
              <Text style={styles.forecastLabel}>30-min forecast</Text>
              <Text style={[styles.forecastValue, { color: getGlucoseColor(latest.forecast_30) }]}>
                {latest.forecast_30} mg/dL
              </Text>
            </View>
            <View style={styles.forecastBox}>
              <Text style={styles.forecastLabel}>60-min forecast</Text>
              <Text style={[styles.forecastValue, { color: getGlucoseColor(latest.forecast_60) }]}>
                {latest.forecast_60} mg/dL
              </Text>
            </View>
          </View>

          {latest.hypo_risk > 40 && (
            <View style={[styles.alertBox, { backgroundColor: latest.hypo_risk > 70 ? '#FEE2E2' : '#FEF3C7' }]}>
              <Text style={styles.alertText}>
                ⚠️ Hypoglycemia risk: {latest.hypo_risk}%{latest.hypo_risk > 70 ? ' — Consider 15g carbs' : ''}
              </Text>
            </View>
          )}

          <View style={styles.statsRow}>
            <Text style={styles.statText}>IOB: {latest.iob.toFixed(1)}u</Text>
            <Text style={styles.statText}>COB: {latest.cob.toFixed(0)}g</Text>
            <Text style={styles.statText}>HR: {latest.hr}bpm</Text>
          </View>
        </View>
      )}

      {chartData.length > 0 && (
        <View style={styles.chartContainer}>
          <Text style={styles.chartTitle}>Last 6 hours</Text>
          <LineChart
            data={{ labels: ['-6h', '-4h', '-2h', 'now'], datasets: [{ data: chartData }] }}
            width={screenWidth - 32}
            height={180}
            chartConfig={{
              backgroundGradientFrom: '#1E3A5F',
              backgroundGradientTo: '#1E3A5F',
              color: (opacity = 1) => `rgba(37, 99, 235, ${opacity})`,
              strokeWidth: 2,
            }}
            bezier
            style={styles.chart}
          />
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0F172A', padding: 16 },
  gaugeContainer: { alignItems: 'center', paddingVertical: 20 },
  glucoseValue: { fontSize: 72, fontWeight: 'bold' },
  glucoseUnit: { fontSize: 18, color: '#94A3B8' },
  trendArrow: { fontSize: 36, marginVertical: 8 },
  forecastRow: { flexDirection: 'row', gap: 16, marginVertical: 16 },
  forecastBox: { alignItems: 'center', padding: 12, backgroundColor: '#1E293B', borderRadius: 8 },
  forecastLabel: { fontSize: 12, color: '#94A3B8' },
  forecastValue: { fontSize: 20, fontWeight: 'bold', marginTop: 4 },
  alertBox: { padding: 12, borderRadius: 8, marginVertical: 8, width: '100%' },
  alertText: { fontSize: 14, fontWeight: '600', textAlign: 'center' },
  statsRow: { flexDirection: 'row', gap: 24, marginVertical: 12 },
  statText: { fontSize: 14, color: '#CBD5E1' },
  chartContainer: { marginTop: 16 },
  chartTitle: { fontSize: 14, color: '#94A3B8', marginBottom: 8 },
  chart: { borderRadius: 8 },
});