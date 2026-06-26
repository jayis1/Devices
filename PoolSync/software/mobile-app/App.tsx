/**
 * PoolSync Mobile App — React Native + Expo
 *
 * Main entry point with navigation structure:
 * - Home: Pool Health Score + quick actions
 * - Chemistry: Real-time chemistry readings with ideal ranges
 * - Camera: Water clarity camera + algae detection
 * - Equipment: Pump/heater schedule + manual control
 * - Alerts: Push notification history + safety events
 * - Settings: Pool config, Wi-Fi, notifications
 */

import React, { useEffect, useState } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Provider as PaperProvider, DefaultTheme } from 'react-native-paper';
import { StatusBar } from 'expo-status-bar';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { LineChart, YAxis, XAxis, Grid } from 'react-native-svg-charts';
import axios from 'axios';
import { registerForPushNotificationsAsync } from './notifications';

const API_BASE = 'https://api.poolsync.io';

// ============================================================
// Theme
// ============================================================

const theme = {
  ...DefaultTheme,
  colors: {
    ...DefaultTheme.colors,
    primary: '#0077B6',      // Pool blue
    accent: '#00B4D8',       // Light blue
    background: '#F0F8FF',   // Alice blue
    surface: '#FFFFFF',
    error: '#E63946',        // Warning red
    warning: '#FFB703',      // Amber
    success: '#2D6A4F',      // Forest green
  },
};

// ============================================================
// Types
// ============================================================

interface ChemistryReading {
  probe_id: number;
  ph: number;
  orp_mv: number;
  free_cl_ppm: number;
  temperature_c: number;
  conductivity_us: number;
  turbidity_ntu: number;
  timestamp: string;
}

interface PoolHealthScore {
  overall: number;
  chemistry: number;
  clarity: number;
  safety: number;
  energy: number;
}

interface AlgaeForecast {
  risk_level: string;
  confidence: number;
  forecast_24h: number;
  forecast_48h: number;
  forecast_72h: number;
  contributing_factors: string[];
}

// ============================================================
// Home Screen — Pool Health Score + Quick Actions
// ============================================================

function HomeScreen() {
  const [health, setHealth] = useState<PoolHealthScore | null>(null);
  const [chemistry, setChemistry] = useState<ChemistryReading | null>(null);
  const [forecast, setForecast] = useState<AlgaeForecast | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetchHealth = async () => {
    try {
      const [healthRes, chemRes, forecastRes] = await Promise.all([
        axios.get(`${API_BASE}/api/health-score`),
        axios.get(`${API_BASE}/api/chemistry?limit=1`),
        axios.get(`${API_BASE}/api/algae-forecast`),
      ]);
      setHealth(healthRes.data);
      setChemistry(chemRes.data[0]);
      setForecast(forecastRes.data);
    } catch (error) {
      console.error('Failed to fetch data:', error);
    }
  };

  useEffect(() => { fetchHealth(); }, []);
  const onRefresh = async () => { setRefreshing(true); await fetchHealth(); setRefreshing(false); };

  const getScoreColor = (score: number) => {
    if (score >= 80) return '#2D6A4F';
    if (score >= 60) return '#FFB703';
    return '#E63946';
  };

  const getRiskColor = (risk: string) => {
    switch (risk) {
      case 'none': return '#2D6A4F';
      case 'low': return '#0077B6';
      case 'medium': return '#FFB703';
      case 'high': return '#E63946';
      default: return '#999';
    }
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <StatusBar style="auto" />

      {/* Pool Health Score */}
      <View style={styles.scoreCard}>
        <Text style={styles.scoreTitle}>Pool Health</Text>
        <Text style={[styles.scoreValue, { color: getScoreColor(health?.overall ?? 0) }]}>
          {health?.overall ?? '--'}
        </Text>
        <Text style={styles.scoreLabel}>out of 100</Text>
      </View>

      {/* Category Scores */}
      <View style={styles.categoryRow}>
        <View style={styles.categoryItem}>
          <Text style={[styles.categoryScore, { color: getScoreColor(health?.chemistry ?? 0) }]}>
            {health?.chemistry ?? '--'}
          </Text>
          <Text style={styles.categoryLabel}>Chemistry</Text>
        </View>
        <View style={styles.categoryItem}>
          <Text style={[styles.categoryScore, { color: getScoreColor(health?.clarity ?? 0) }]}>
            {health?.clarity ?? '--'}
          </Text>
          <Text style={styles.categoryLabel}>Clarity</Text>
        </View>
        <View style={styles.categoryItem}>
          <Text style={[styles.categoryScore, { color: getScoreColor(health?.safety ?? 0) }]}>
            {health?.safety ?? '--'}
          </Text>
          <Text style={styles.categoryLabel}>Safety</Text>
        </View>
        <View style={styles.categoryItem}>
          <Text style={[styles.categoryScore, { color: getScoreColor(health?.energy ?? 0) }]}>
            {health?.energy ?? '--'}
          </Text>
          <Text style={styles.categoryLabel}>Energy</Text>
        </View>
      </View>

      {/* Current Chemistry */}
      {chemistry && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Current Chemistry</Text>
          <View style={styles.readingRow}>
            <Text style={styles.readingLabel}>pH</Text>
            <Text style={styles.readingValue}>{chemistry.ph.toFixed(2)}</Text>
            <Text style={styles.readingIdeal}>7.2 – 7.6</Text>
          </View>
          <View style={styles.readingRow}>
            <Text style={styles.readingLabel}>Free Cl</Text>
            <Text style={styles.readingValue}>{chemistry.free_cl_ppm.toFixed(2)} ppm</Text>
            <Text style={styles.readingIdeal}>2.0 – 4.0</Text>
          </View>
          <View style={styles.readingRow}>
            <Text style={styles.readingLabel}>ORP</Text>
            <Text style={styles.readingValue}>{chemistry.orp_mv.toFixed(0)} mV</Text>
            <Text style={styles.readingIdeal}>650 – 800</Text>
          </View>
          <View style={styles.readingRow}>
            <Text style={styles.readingLabel}>Temp</Text>
            <Text style={styles.readingValue}>{chemistry.temperature_c.toFixed(1)}°C</Text>
            <Text style={styles.readingIdeal}>26 – 30°C</Text>
          </View>
          <View style={styles.readingRow}>
            <Text style={styles.readingLabel}>Turbidity</Text>
            <Text style={styles.readingValue}>{chemistry.turbidity_ntu.toFixed(2)} NTU</Text>
            <Text style={styles.readingIdeal}>0 – 0.5</Text>
          </View>
        </View>
      )}

      {/* Algae Forecast */}
      {forecast && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>3-Day Algae Forecast</Text>
          <Text style={[styles.riskLevel, { color: getRiskColor(forecast.risk_level) }]}>
            Risk: {forecast.risk_level.toUpperCase()}
          </Text>
          <View style={styles.forecastRow}>
            <View style={styles.forecastItem}>
              <Text style={styles.forecastValue}>{(forecast.forecast_24h * 100).toFixed(0)}%</Text>
              <Text style={styles.forecastLabel}>24h</Text>
            </View>
            <View style={styles.forecastItem}>
              <Text style={styles.forecastValue}>{(forecast.forecast_48h * 100).toFixed(0)}%</Text>
              <Text style={styles.forecastLabel}>48h</Text>
            </View>
            <View style={styles.forecastItem}>
              <Text style={styles.forecastValue}>{(forecast.forecast_72h * 100).toFixed(0)}%</Text>
              <Text style={styles.forecastLabel}>72h</Text>
            </View>
          </View>
          {forecast.contributing_factors.map((factor, i) => (
            <Text key={i} style={styles.factorText}>• {factor}</Text>
          ))}
        </View>
      )}

      {/* Quick Actions */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Quick Actions</Text>
        <View style={styles.actionRow}>
          <Text style={[styles.actionButton, { backgroundColor: '#0077B6' }]}>
            💦 Shock Treatment
          </Text>
          <Text style={[styles.actionButton, { backgroundColor: '#FFB703' }]}>
            🏖️ Vacation Mode
          </Text>
        </View>
      </View>
    </ScrollView>
  );
}

// ============================================================
// Chemistry Screen — Detailed readings + history charts
// ============================================================

function ChemistryScreen() {
  const [readings, setReadings] = useState<ChemistryReading[]>([]);
  const [idealRanges, setIdealRanges] = useState<any>(null);

  useEffect(() => {
    const fetch = async () => {
      try {
        const [chemRes, rangeRes] = await Promise.all([
          axios.get(`${API_BASE}/api/chemistry?limit=288`), // 24h at 5-min intervals
          axios.get(`${API_BASE}/api/chemistry/ideal`),
        ]);
        setReadings(chemRes.data);
        setIdealRanges(rangeRes.data);
      } catch (e) { console.error(e); }
    };
    fetch();
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.screenTitle}>Chemistry</Text>
      {/* pH, Cl, ORP, Temp, Conductivity, Turbidity charts */}
      {/* Ideal range bands overlaid on charts */}
      {/* Each reading with color-coded status (green=ideal, yellow=acceptable, red=danger) */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>pH History (24h)</Text>
        {/* Line chart with ideal range overlay */}
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Free Chlorine History (24h)</Text>
        {/* Line chart */}
      </View>
      {idealRanges && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Ideal Ranges</Text>
          {Object.entries(idealRanges).map(([key, range]: [string, any]) => (
            <View key={key} style={styles.readingRow}>
              <Text style={styles.readingLabel}>{key}</Text>
              <Text style={styles.readingValue}>
                {range.min} – {range.max} {range.unit}
              </Text>
            </View>
          ))}
        </View>
      )}
    </ScrollView>
  );
}

// ============================================================
// App with Navigation
// ============================================================

const Tab = createBottomTabNavigator();

export default function App() {
  useEffect(() => {
    registerForPushNotificationsAsync();
  }, []);

  return (
    <PaperProvider theme={theme}>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={{
            tabBarActiveTintColor: theme.colors.primary,
            tabBarInactiveTintColor: '#999',
          }}
        >
          <Tab.Screen name="Home" component={HomeScreen}
            options={{ tabBarIcon: 'home', tabBarLabel: 'Home' }} />
          <Tab.Screen name="Chemistry" component={ChemistryScreen}
            options={{ tabBarIcon: 'flask', tabBarLabel: 'Chemistry' }} />
          <Tab.Screen name="Equipment" component={HomeScreen}
            options={{ tabBarIcon: 'cog', tabBarLabel: 'Equipment' }} />
          <Tab.Screen name="Alerts" component={HomeScreen}
            options={{ tabBarIcon: 'bell', tabBarLabel: 'Alerts' }} />
          <Tab.Screen name="Settings" component={HomeScreen}
            options={{ tabBarIcon: 'cog-outline', tabBarLabel: 'Settings' }} />
        </Tab.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}

// ============================================================
// Styles
// ============================================================

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#F0F8FF' },
  scoreCard: {
    alignItems: 'center', padding: 24,
    backgroundColor: '#fff', margin: 16, borderRadius: 16,
    shadowColor: '#000', shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1, shadowRadius: 8, elevation: 4,
  },
  scoreTitle: { fontSize: 18, fontWeight: '600', color: '#333' },
  scoreValue: { fontSize: 72, fontWeight: '700', marginVertical: 8 },
  scoreLabel: { fontSize: 14, color: '#666' },
  categoryRow: {
    flexDirection: 'row', justifyContent: 'space-around',
    marginHorizontal: 16, marginBottom: 16,
  },
  categoryItem: { alignItems: 'center' },
  categoryScore: { fontSize: 28, fontWeight: '700' },
  categoryLabel: { fontSize: 12, color: '#666', marginTop: 4 },
  card: {
    backgroundColor: '#fff', marginHorizontal: 16, marginBottom: 16,
    padding: 16, borderRadius: 12,
    shadowColor: '#000', shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1, shadowRadius: 4, elevation: 2,
  },
  cardTitle: { fontSize: 16, fontWeight: '600', color: '#333', marginBottom: 12 },
  readingRow: {
    flexDirection: 'row', justifyContent: 'space-between',
    paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#f0f0f0',
  },
  readingLabel: { fontSize: 14, color: '#666', width: 80 },
  readingValue: { fontSize: 14, fontWeight: '500', color: '#333', flex: 1 },
  readingIdeal: { fontSize: 12, color: '#999', width: 80, textAlign: 'right' },
  riskLevel: { fontSize: 20, fontWeight: '700', marginBottom: 12 },
  forecastRow: { flexDirection: 'row', justifyContent: 'space-around', marginVertical: 12 },
  forecastItem: { alignItems: 'center' },
  forecastValue: { fontSize: 24, fontWeight: '600', color: '#333' },
  forecastLabel: { fontSize: 12, color: '#666', marginTop: 4 },
  factorText: { fontSize: 13, color: '#555', marginVertical: 2 },
  actionRow: { flexDirection: 'row', justifyContent: 'space-around', marginVertical: 8 },
  actionButton: {
    paddingVertical: 12, paddingHorizontal: 20, borderRadius: 8,
    color: '#fff', fontWeight: '600', fontSize: 14, overflow: 'hidden',
  },
  screenTitle: { fontSize: 24, fontWeight: '700', color: '#333', margin: 16 },
});