/**
 * AllergySync — Dashboard Screen
 * Current pollen levels, personal risk meter, 24-hour forecast
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';

const API_BASE = 'https://api.allergysync.io/api/v1';

interface ExposureData {
  pm2_5: number;
  pm10: number;
  co2_ppm: number;
  pollen_class: number;
  pollen_name: string;
  pollen_confidence: number;
  risk_level: string;
  timestamp: string;
}

const POLLEN_COLORS: Record<string, string> = {
  none: '#4CAF50',
  birch: '#8BC34A',
  grass: '#CDDC39',
  ragweed: '#FF9800',
  oak: '#FF5722',
  pine: '#FFC107',
  mold: '#9C27B0',
};

const RISK_COLORS: Record<string, string> = {
  low: '#4CAF50',
  moderate: '#FF9800',
  high: '#F44336',
};

export default function DashboardScreen() {
  const [data, setData] = useState<ExposureData | null>(null);
  const [forecast, setForecast] = useState<any[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const [expRes, fcRes] = await Promise.all([
        fetch(`${API_BASE}/exposure/current`),
        fetch(`${API_BASE}/exposure/forecast`),
      ]);
      const exp = await expRes.json();
      const fc = await fcRes.json();
      setData(exp);
      setForecast(fc.forecast || []);
    } catch (err) {
      console.error('Fetch error:', err);
    }
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 60000);
    return () => clearInterval(interval);
  }, [fetchData]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  }, [fetchData]);

  const riskColor = data ? RISK_COLORS[data.risk_level] || '#666' : '#666';
  const pollenColor = data ? POLLEN_COLORS[data.pollen_name] || '#666' : '#666';

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Risk Meter */}
      <View style={[styles.card, { borderLeftColor: riskColor }]}>
        <Text style={styles.cardTitle}>Allergen Risk</Text>
        <Text style={[styles.riskLevel, { color: riskColor }]}>
          {data?.risk_level?.toUpperCase() || '—'}
        </Text>
        <Text style={styles.subtitle}>
          {data ? `Updated ${new Date(data.timestamp).toLocaleTimeString()}` : 'Loading...'}
        </Text>
      </View>

      {/* Current Pollen */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Current Pollen</Text>
        <View style={styles.pollenRow}>
          <View style={[styles.pollenBadge, { backgroundColor: pollenColor }]}>
            <Text style={styles.pollenName}>
              {data?.pollen_name?.toUpperCase() || '—'}
            </Text>
          </View>
          <Text style={styles.confidence}>
            {data ? `${data.pollen_confidence}% confidence` : ''}
          </Text>
        </View>
      </View>

      {/* Air Quality */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Air Quality</Text>
        <View style={styles.metricRow}>
          <View style={styles.metric}>
            <Text style={styles.metricValue}>{data?.pm2_5?.toFixed(1) || '—'}</Text>
            <Text style={styles.metricLabel}>PM2.5 µg/m³</Text>
          </View>
          <View style={styles.metric}>
            <Text style={styles.metricValue}>{data?.pm10?.toFixed(1) || '—'}</Text>
            <Text style={styles.metricLabel}>PM10 µg/m³</Text>
          </View>
          <View style={styles.metric}>
            <Text style={styles.metricValue}>{data?.co2_ppm || '—'}</Text>
            <Text style={styles.metricLabel}>CO₂ ppm</Text>
          </View>
        </View>
      </View>

      {/* 24-Hour Forecast */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>24-Hour Forecast</Text>
        <ScrollView horizontal showsHorizontalScrollIndicator={false}>
          {forecast.map((f, i) => (
            <View key={i} style={styles.forecastHour}>
              <Text style={styles.forecastTime}>{f.hour}:00</Text>
              <View style={[styles.forecastDot,
                { backgroundColor: POLLEN_COLORS[Object.keys(POLLEN_COLORS)[f.pollen_class]] || '#ccc' }]} />
              <Text style={styles.forecastConc}>{f.concentration}</Text>
            </View>
          ))}
        </ScrollView>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: {
    backgroundColor: 'white', margin: 12, padding: 16,
    borderRadius: 12, borderLeftWidth: 4, borderLeftColor: '#ddd',
    shadowColor: '#000', shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1, shadowRadius: 4, elevation: 2,
  },
  cardTitle: { fontSize: 16, fontWeight: '600', color: '#333', marginBottom: 8 },
  riskLevel: { fontSize: 32, fontWeight: 'bold' },
  subtitle: { fontSize: 12, color: '#999', marginTop: 4 },
  pollenRow: { flexDirection: 'row', alignItems: 'center', gap: 12 },
  pollenBadge: { paddingHorizontal: 16, paddingVertical: 8, borderRadius: 20 },
  pollenName: { color: 'white', fontWeight: 'bold', fontSize: 16 },
  confidence: { fontSize: 14, color: '#666' },
  metricRow: { flexDirection: 'row', justifyContent: 'space-around' },
  metric: { alignItems: 'center' },
  metricValue: { fontSize: 24, fontWeight: 'bold', color: '#333' },
  metricLabel: { fontSize: 12, color: '#999', marginTop: 4 },
  forecastHour: { alignItems: 'center', marginHorizontal: 8, width: 60 },
  forecastTime: { fontSize: 12, color: '#666' },
  forecastDot: { width: 20, height: 20, borderRadius: 10, marginVertical: 4 },
  forecastConc: { fontSize: 10, color: '#999' },
});