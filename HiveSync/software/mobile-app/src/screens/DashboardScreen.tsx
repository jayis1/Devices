/**
 * HiveSync — Dashboard Screen
 * Main apiary overview with hive health scores and weather
 */

import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, RefreshControl } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { useApi } from '../context/ApiContext';
import { useAlerts } from '../context/AlertContext';
import { HiveHealthCard } from '../components/HiveHealthCard';
import { WeatherWidget } from '../components/WeatherWidget';
import { AlertBanner } from '../components/AlertBanner';
import { SwarmRiskGauge } from '../components/SwarmRiskGauge';

interface ApiaryDashboard {
  apiary_id: string;
  hives: HiveSummary[];
  weather: WeatherData;
  summary: {
    total_hives: number;
    healthy: number;
    attention_needed: number;
    critical: number;
  };
}

interface HiveSummary {
  id: string;
  name: string;
  health_score: number;
  swarm_risk: number;
  mite_level: string;
  queen_status: string;
  last_reading: string;
}

export default function DashboardScreen() {
  const { api } = useApi();
  const { unreadAlerts } = useAlerts();
  const [dashboard, setDashboard] = useState<ApiaryDashboard | null>(null);
  const [refreshing, setRefreshing] = useState(false);
  const navigation = useNavigation();

  const fetchDashboard = async () => {
    try {
      const data = await api.get('/api/v1/apiaries/current/dashboard');
      setDashboard(data);
    } catch (err) {
      console.error('Failed to load dashboard', err);
    }
  };

  useEffect(() => { fetchDashboard(); }, []);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchDashboard();
    setRefreshing(false);
  };

  if (!dashboard) {
    return (
      <View style={styles.loading}>
        <Text style={styles.loadingText}>Loading hives...</Text>
      </View>
    );
  }

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Alert banner */}
      {unreadAlerts.length > 0 && (
        <AlertBanner alerts={unreadAlerts} onPress={() => navigation.navigate('Alerts')} />
      )}

      {/* Summary cards */}
      <View style={styles.summaryRow}>
        <View style={[styles.summaryCard, { backgroundColor: '#4CAF50' }]}>
          <Text style={styles.summaryNumber}>{dashboard.summary.healthy}</Text>
          <Text style={styles.summaryLabel}>Healthy</Text>
        </View>
        <View style={[styles.summaryCard, { backgroundColor: '#FF9800' }]}>
          <Text style={styles.summaryNumber}>{dashboard.summary.attention_needed}</Text>
          <Text style={styles.summaryLabel}>Attention</Text>
        </View>
        <View style={[styles.summaryCard, { backgroundColor: '#F44336' }]}>
          <Text style={styles.summaryNumber}>{dashboard.summary.critical}</Text>
          <Text style={styles.summaryLabel}>Critical</Text>
        </View>
      </View>

      {/* Weather */}
      <WeatherWidget weather={dashboard.weather} />

      {/* Hive cards */}
      <Text style={styles.sectionTitle}>Hives</Text>
      {dashboard.hives.map((hive) => (
        <HiveHealthCard
          key={hive.id}
          hive={hive}
          onPress={() => navigation.navigate('HiveDetail', { hiveId: hive.id })}
        />
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  loading: { flex: 1, justifyContent: 'center', alignItems: 'center', backgroundColor: '#1a1a2e' },
  loadingText: { color: '#fff', fontSize: 18 },
  summaryRow: { flexDirection: 'row', justifyContent: 'space-around', padding: 16 },
  summaryCard: { padding: 16, borderRadius: 12, alignItems: 'center', minWidth: 90 },
  summaryNumber: { fontSize: 28, fontWeight: 'bold', color: '#fff' },
  summaryLabel: { fontSize: 12, color: '#fff', opacity: 0.8 },
  sectionTitle: { color: '#fff', fontSize: 20, fontWeight: 'bold', marginLeft: 16, marginTop: 16 },
});