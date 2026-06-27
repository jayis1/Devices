import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { useStore } from '../store/store';
import MaturityGauge from '../components/MaturityGauge';
import SensorCard from '../components/SensorCard';
import PhaseIndicator from '../components/PhaseIndicator';

export default function DashboardScreen() {
  const { compostStatus, telemetry, fetchStatus, loading } = useStore();
  const [refreshing, setRefreshing] = useState(false);

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 60000);
    return () => clearInterval(interval);
  }, []);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchStatus();
    setRefreshing(false);
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <View style={styles.header}>
        <Text style={styles.title}>🌱 CompostSync</Text>
        <Text style={styles.subtitle}>{compostStatus?.device_id || 'Connecting...'}</Text>
      </View>

      {/* Maturity Gauge */}
      <MaturityGauge score={compostStatus?.maturity_score || 0} />

      {/* Phase Indicator */}
      <PhaseIndicator phase={compostStatus?.phase || 'unknown'} />

      {/* Recommendation */}
      {compostStatus?.recommendation && (
        <View style={styles.recommendationCard}>
          <Text style={styles.recommendationText}>{compostStatus.recommendation}</Text>
        </View>
      )}

      {/* Key Metrics */}
      <View style={styles.metricsRow}>
        <SensorCard
          icon="🌡️"
          label="Temperature"
          value={telemetry ? `${(telemetry.temp_c?.[1] / 10 || 0).toFixed(1)}°C` : '--'}
          status={getTempStatus(telemetry?.temp_c?.[1] || 0)}
        />
        <SensorCard
          icon="💧"
          label="Moisture"
          value={telemetry ? `${telemetry.moisture_pct?.[1] || 0}%` : '--'}
          status={getMoistureStatus(telemetry?.moisture_pct?.[1] || 0)}
        />
      </View>

      <View style={styles.metricsRow}>
        <SensorCard
          icon="🫧"
          label="CO₂"
          value={telemetry ? `${telemetry.co2_ppm} ppm` : '--'}
          status="good"
        />
        <SensorCard
          icon="💨"
          label="Methane"
          value={telemetry ? `${telemetry.methane_ppm} ppm` : '--'}
          status={telemetry && telemetry.methane_ppm > 500 ? 'bad' : 'good'}
        />
      </View>

      <View style={styles.metricsRow}>
        <SensorCard
          icon="⚖️"
          label="Mass"
          value={telemetry ? `${(telemetry.mass_grams / 1000).toFixed(1)} kg` : '--'}
          status="good"
        />
        <SensorCard
          icon="🔋"
          label="Battery"
          value={telemetry ? `${telemetry.battery_pct}%` : '--'}
          status={telemetry && telemetry.battery_pct < 20 ? 'bad' : 'good'}
        />
      </View>

      {/* C:N + Days to Ready */}
      <View style={styles.infoRow}>
        <View style={styles.infoCard}>
          <Text style={styles.infoLabel}>C:N Ratio</Text>
          <Text style={styles.infoValue}>{compostStatus?.cn_ratio?.toFixed(0) || '?'}:1</Text>
        </View>
        <View style={styles.infoCard}>
          <Text style={styles.infoLabel}>Days to Ready</Text>
          <Text style={styles.infoValue}>{compostStatus?.days_to_ready ?? '?'}</Text>
        </View>
        <View style={styles.infoCard}>
          <Text style={styles.infoLabel}>Diverted</Text>
          <Text style={styles.infoValue}>{compostStatus?.diverted_kg?.toFixed(1) || '0'} kg</Text>
        </View>
      </View>
    </ScrollView>
  );
}

function getTempStatus(temp10: number): 'good' | 'warning' | 'bad' {
  const temp = temp10 / 10;
  if (temp > 70) return 'bad';
  if (temp > 50 && temp <= 70) return 'good';
  if (temp < 20) return 'warning';
  return 'good';
}

function getMoistureStatus(moisture: number): 'good' | 'warning' | 'bad' {
  if (moisture > 70) return 'bad';
  if (moisture < 30) return 'warning';
  return 'good';
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#121212' },
  header: { padding: 20, alignItems: 'center' },
  title: { fontSize: 28, fontWeight: 'bold', color: '#4CAF50' },
  subtitle: { fontSize: 14, color: '#888', marginTop: 4 },
  recommendationCard: {
    margin: 16,
    padding: 16,
    backgroundColor: '#1E2A1E',
    borderRadius: 12,
    borderLeftWidth: 4,
    borderLeftColor: '#4CAF50',
  },
  recommendationText: { color: '#E0E0E0', fontSize: 15, lineHeight: 22 },
  metricsRow: { flexDirection: 'row', justifyContent: 'space-around', paddingHorizontal: 12 },
  infoRow: { flexDirection: 'row', justifyContent: 'space-around', paddingHorizontal: 12, marginTop: 12, marginBottom: 20 },
  infoCard: { alignItems: 'center', padding: 12 },
  infoLabel: { fontSize: 12, color: '#888' },
  infoValue: { fontSize: 18, fontWeight: 'bold', color: '#fff', marginTop: 4 },
});