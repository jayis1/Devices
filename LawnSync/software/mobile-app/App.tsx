/**
 * LawnSync Mobile App — Dashboard Screen
 *
 * Main screen showing Lawn Health Score, zone moisture map,
 * today's irrigation, active alerts, and water savings.
 *
 * React Native + Expo + React Native Paper
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  View, Text, StyleSheet, ScrollView, RefreshControl,
  TouchableOpacity, Image, Dimensions
} from 'react-native';
import { Card, Title, Paragraph, ProgressBar, Button, IconButton,
  ActivityIndicator, Divider, Surface, useTheme } from 'react-native-paper';
import { LineChart, BarChart } from 'react-native-chart-kit';

const API_BASE = 'https://api.lawnsync.cloud/api/v1';
const screenWidth = Dimensions.get('window').width;

// ---- Types ----
interface HealthScore {
  score: number;
  status: string;
  moisture_score: number;
  disease_score: number;
  nutrient_score: number;
  density_score: number;
  recommendations: string[];
}

interface SoilReading {
  node_id: number;
  moisture_pct: number;
  temp_c: number;
  ph: number;
  nitrogen_mgkg: number;
  phosphorus_mgkg: number;
  potassium_mgkg: number;
  battery_mv: number;
}

interface Alert {
  id: string;
  node_id: number;
  alert_type: string;
  severity: number;
  message: string;
  timestamp: string;
  acknowledged: boolean;
}

interface WaterUsage {
  today_liters: number;
  week_liters: number;
  month_liters: number;
  savings_vs_timer_liters: number;
  savings_pct: number;
}

// ---- Helper ----
const scoreColor = (score: number): string => {
  if (score >= 80) return '#2E7D32';  // green
  if (score >= 60) return '#F9A825';  // amber
  if (score >= 40) return '#EF6C00';  // orange
  return '#C62828';                    // red
};

const severityIcon = (sev: number): string => {
  if (sev === 1) return 'information';
  if (sev === 2) return 'alert-circle';
  if (sev === 3) return 'alert-octagon';
  return 'bell';
};

const severityColor = (sev: number): string => {
  if (sev === 1) return '#2196F3';
  if (sev === 2) return '#FF9800';
  if (sev === 3) return '#F44336';
  return '#9E9E9E';
};

// ---- HealthScoreGauge ----
const HealthScoreGauge: React.FC<{ score: number; status: string }> = ({ score, status }) => {
  const color = scoreColor(score);
  return (
    <View style={styles.gaugeContainer}>
      <Surface style={[styles.gauge, { borderColor: color }]} elevation={4}>
        <Text style={[styles.gaugeScore, { color }]}>{score}</Text>
        <Text style={styles.gaugeLabel}>Health Score</Text>
        <Text style={[styles.gaugeStatus, { color }]}>{status}</Text>
      </Surface>
    </View>
  );
};

// ---- ScoreBar ----
const ScoreBar: React.FC<{ label: string; value: number }> = ({ label, value }) => (
  <View style={styles.scoreBarRow}>
    <Text style={styles.scoreBarLabel}>{label}</Text>
    <ProgressBar
      progress={value / 100}
      color={scoreColor(value)}
      style={styles.scoreBar}
    />
    <Text style={styles.scoreBarValue}>{value}</Text>
  </View>
);

// ---- ZoneMoistureMap ----
const ZoneMoistureMap: React.FC<{ readings: SoilReading[] }> = ({ readings }) => {
  const moistureColor = (m: number): string => {
    if (m < 12) return '#C62828';
    if (m < 18) return '#EF6C00';
    if (m <= 28) return '#2E7D32';
    if (m <= 35) return '#1565C0';
    return '#0D47A1';
  };

  return (
    <Card style={styles.card}>
      <Card.Content>
        <Title>Soil Moisture by Zone</Title>
        {readings.map(r => (
          <View key={r.node_id} style={styles.zoneRow}>
            <View style={styles.zoneInfo}>
              <Text style={styles.zoneName}>Zone {r.node_id}</Text>
              <Text style={styles.zoneDetail}>{r.moisture_pct.toFixed(1)}% • {r.temp_c.toFixed(1)}°C</Text>
            </View>
            <View style={[styles.zoneBar, { backgroundColor: moistureColor(r.moisture_pct) }]}>
              <Text style={styles.zoneBarText}>{r.moisture_pct.toFixed(0)}%</Text>
            </View>
          </View>
        ))}
      </Card.Content>
    </Card>
  );
};

// ---- AlertCard ----
const AlertCard: React.FC<{ alert: Alert; onPress: (id: string) => void }> = ({ alert, onPress }) => (
  <Card style={[styles.alertCard, { borderLeftColor: severityColor(alert.severity) }]}>
    <Card.Content style={styles.alertContent}>
      <IconButton
        icon={severityIcon(alert.severity)}
        size={24}
        iconColor={severityColor(alert.severity)}
      />
      <View style={styles.alertText}>
        <Text style={styles.alertMessage}>{alert.message}</Text>
        <Text style={styles.alertTime}>{new Date(alert.timestamp).toLocaleString()}</Text>
      </View>
      {!alert.acknowledged && (
        <Button mode="text" onPress={() => onPress(alert.id)}>Dismiss</Button>
      )}
    </Card.Content>
  </Card>
);

// ---- Main Dashboard ----
const DashboardScreen: React.FC<{ navigation: any }> = ({ navigation }) => {
  const [health, setHealth] = useState<HealthScore | null>(null);
  const [soil, setSoil] = useState<SoilReading[]>([]);
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [water, setWater] = useState<WaterUsage | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAll = useCallback(async () => {
    try {
      const [healthR, soilR, alertR, waterR] = await Promise.all([
        fetch(`${API_BASE}/health-score`).then(r => r.json()),
        fetch(`${API_BASE}/soil`).then(r => r.json()),
        fetch(`${API_BASE}/alerts?acknowledged=false`).then(r => r.json()),
        fetch(`${API_BASE}/water-usage`).then(r => r.json()),
      ]);
      setHealth(healthR);
      setSoil(soilR);
      setAlerts(alertR);
      setWater(waterR);
    } catch (e) {
      console.error('Fetch error:', e);
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, []);

  useEffect(() => { fetchAll(); }, [fetchAll]);

  const acknowledgeAlert = async (id: string) => {
    await fetch(`${API_BASE}/alerts/${id}/ack`, { method: 'PUT' });
    setAlerts(prev => prev.filter(a => a.id !== id));
  };

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" />
        <Text style={styles.loadingText}>Loading your lawn...</Text>
      </View>
    );
  }

  return (
    <ScrollView
      style={styles.container}
      refreshControl={
        <RefreshControl refreshing={refreshing} onRefresh={() => { setRefreshing(true); fetchAll(); }} />
      }
    >
      {/* Health Score Gauge */}
      {health && (
        <>
          <HealthScoreGauge score={health.score} status={health.status} />

          {/* Sub-scores */}
          <Card style={styles.card}>
            <Card.Content>
              <Title>Detailed Scores</Title>
              <ScoreBar label="Moisture" value={health.moisture_score} />
              <ScoreBar label="Disease" value={health.disease_score} />
              <ScoreBar label="Nutrients" value={health.nutrient_score} />
              <ScoreBar label="Density" value={health.density_score} />
            </Card.Content>
          </Card>

          {/* Recommendations */}
          <Card style={styles.card}>
            <Card.Content>
              <Title>Recommendations</Title>
              {health.recommendations.map((rec, i) => (
                <View key={i} style={styles.recRow}>
                  <IconButton icon="lightbulb-outline" size={20} iconColor="#F9A825" />
                  <Text style={styles.recText}>{rec}</Text>
                </View>
              ))}
            </Card.Content>
          </Card>
        </>
      )}

      {/* Zone Moisture Map */}
      <ZoneMoistureMap readings={soil} />

      {/* Active Alerts */}
      {alerts.length > 0 && (
        <Card style={styles.card}>
          <Card.Content>
            <Title>Active Alerts ({alerts.length})</Title>
          </Card.Content>
          {alerts.slice(0, 5).map(a => (
            <AlertCard key={a.id} alert={a} onPress={acknowledgeAlert} />
          ))}
        </Card>
      )}

      {/* Water Savings */}
      {water && (
        <Card style={styles.card}>
          <Card.Content>
            <Title>Water Savings</Title>
            <View style={styles.waterRow}>
              <View style={styles.waterCell}>
                <Text style={styles.waterValue}>{water.savings_pct}%</Text>
                <Text style={styles.waterLabel}>Saved</Text>
              </View>
              <View style={styles.waterCell}>
                <Text style={styles.waterValue}>{water.savings_vs_timer_liters}L</Text>
                <Text style={styles.waterLabel}>This Month</Text>
              </View>
              <View style={styles.waterCell}>
                <Text style={styles.waterValue}>{water.today_liters}L</Text>
                <Text style={styles.waterLabel}>Today</Text>
              </View>
            </View>
          </Card.Content>
        </Card>
      )}

      {/* Quick Actions */}
      <View style={styles.quickActions}>
        <Button mode="contained" icon="water" onPress={() => navigation.navigate('Irrigation')}
                style={styles.actionBtn}>
          Irrigation
        </Button>
        <Button mode="contained" icon="grass" onPress={() => navigation.navigate('Scanner')}
                style={styles.actionBtn}>
          Scanner
        </Button>
      </View>

      <Divider />
      <Text style={styles.footer}>LawnSync v1.0 • Powered by AI</Text>
    </ScrollView>
  );
};

// ---- Zones Screen ----
const ZonesScreen: React.FC = () => {
  const [soil, setSoil] = useState<SoilReading[]>([]);

  useEffect(() => {
    fetch(`${API_BASE}/soil`).then(r => r.json()).then(setSoil);
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Title style={styles.screenTitle}>Zones</Title>
      {soil.map(r => (
        <Card key={r.node_id} style={styles.card}>
          <Card.Content>
            <Title>Zone {r.node_id}</Title>
            <Paragraph>Moisture: {r.moisture_pct.toFixed(1)}%</Paragraph>
            <Paragraph>Temperature: {r.temp_c.toFixed(1)}°C</Paragraph>
            <Paragraph>pH: {r.ph.toFixed(1)}</Paragraph>
            <Paragraph>N: {r.nitrogen_mgkg.toFixed(0)} mg/kg</Paragraph>
            <Paragraph>P: {r.phosphorus_mgkg.toFixed(0)} mg/kg</Paragraph>
            <Paragraph>K: {r.potassium_mgkg.toFixed(0)} mg/kg</Paragraph>
            <Paragraph>Battery: {(r.battery_mv / 100).toFixed(2)}V</Paragraph>
          </Card.Content>
        </Card>
      ))}
    </ScrollView>
  );
};

// ---- Irrigation Screen ----
const IrrigationScreen: React.FC = () => {
  const [schedule, setSchedule] = useState<any>(null);

  useEffect(() => {
    fetch(`${API_BASE}/irrigation/schedule`).then(r => r.json()).then(setSchedule);
  }, []);

  const runZone = async (zoneId: number) => {
    await fetch(`${API_BASE}/irrigation/zone/${zoneId}/run?duration_min=10`, { method: 'POST' });
    alert(`Zone ${zoneId} started for 10 minutes`);
  };

  return (
    <ScrollView style={styles.container}>
      <Title style={styles.screenTitle}>Irrigation</Title>
      {schedule?.zones?.map((z: any) => (
        <Card key={z.zone_id} style={styles.card}>
          <Card.Content>
            <Title>{z.name}</Title>
            <Paragraph>Enabled: {z.enabled ? 'Yes' : 'No'}</Paragraph>
            <Paragraph>Duration: {z.duration_min} min</Paragraph>
            <Paragraph>Days: {z.days.join(', ')}</Paragraph>
            <Paragraph>Start: {z.start_time}</Paragraph>
            <Paragraph>Moisture threshold: {z.moisture_threshold_pct}%</Paragraph>
            <Button mode="outlined" onPress={() => runZone(z.zone_id)} style={styles.runBtn}>
              Run Now (10 min)
            </Button>
          </Card.Content>
        </Card>
      ))}
      {schedule && (
        <Card style={styles.card}>
          <Card.Content>
            <Title>Water Savings</Title>
            <Paragraph>Saved this month: {schedule.water_saved_liters}L ({schedule.water_saved_pct}%)</Paragraph>
          </Card.Content>
        </Card>
      )}
    </ScrollView>
  );
};

// ---- Scanner Screen ----
const ScannerScreen: React.FC = () => {
  const [results, setResults] = useState<any[]>([]);

  useEffect(() => {
    fetch(`${API_BASE}/scan/results`).then(r => r.json()).then(setResults);
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Title style={styles.screenTitle}>Lawn Scanner</Title>
      {results.map((r, i) => (
        <Card key={i} style={styles.card}>
          <Card.Content>
            <Title>{r.disease_class}</Title>
            <Paragraph>Confidence: {(r.confidence * 100).toFixed(0)}%</Paragraph>
            <Paragraph>NDVI: {r.avg_ndvi.toFixed(2)}</Paragraph>
            <Paragraph>Weed coverage: {r.weed_coverage_pct}%</Paragraph>
            <Paragraph>Dominant weed: {r.dominant_weed}</Paragraph>
            <Paragraph>{new Date(r.timestamp).toLocaleString()}</Paragraph>
          </Card.Content>
        </Card>
      ))}
    </ScrollView>
  );
};

// ---- Weather Screen ----
const WeatherScreen: React.FC = () => {
  const [weather, setWeather] = useState<any>(null);

  useEffect(() => {
    fetch(`${API_BASE}/weather`).then(r => r.json()).then(setWeather);
  }, []);

  if (!weather) return <ActivityIndicator style={styles.centered} size="large" />;

  return (
    <ScrollView style={styles.container}>
      <Title style={styles.screenTitle}>Weather</Title>
      <Card style={styles.card}>
        <Card.Content>
          <Title>{weather.temp_c.toFixed(1)}°C</Title>
          <Paragraph>Humidity: {weather.humidity_pct.toFixed(0)}%</Paragraph>
          <Paragraph>Pressure: {weather.pressure_hpa.toFixed(1)} hPa</Paragraph>
          <Paragraph>Wind: {weather.wind_speed_ms.toFixed(1)} m/s ({weather.wind_dir_deg}°)</Paragraph>
          <Paragraph>Rain: {weather.rain_mm.toFixed(1)} mm</Paragraph>
          <Paragraph>Solar: {weather.solar_irr_wm2} W/m²</Paragraph>
          <Paragraph>UV Index: {weather.uv_index.toFixed(1)}</Paragraph>
        </Card.Content>
      </Card>
    </ScrollView>
  );
};

// ---- App Entry ----
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Provider as PaperProvider } from 'react-native-paper';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <PaperProvider>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={{
            tabBarActiveTintColor: '#2E7D32',
            headerStyle: { backgroundColor: '#2E7D32' },
            headerTintColor: '#fff',
          }}
        >
          <Tab.Screen name="Dashboard" component={DashboardScreen}
            options={{ tabBarIcon: 'home' }} />
          <Tab.Screen name="Zones" component={ZonesScreen}
            options={{ tabBarIcon: 'map-marker' }} />
          <Tab.Screen name="Irrigation" component={IrrigationScreen}
            options={{ tabBarIcon: 'water' }} />
          <Tab.Screen name="Scanner" component={ScannerScreen}
            options={{ tabBarIcon: 'grass' }} />
          <Tab.Screen name="Weather" component={WeatherScreen}
            options={{ tabBarIcon: 'weather-sunny' }} />
        </Tab.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}

// ---- Styles ----
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA', padding: 8 },
  centered: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  loadingText: { marginTop: 10, fontSize: 16 },
  screenTitle: { fontSize: 24, fontWeight: 'bold', margin: 12, color: '#2E7D32' },
  card: { marginBottom: 12, borderRadius: 12 },
  gaugeContainer: { alignItems: 'center', padding: 20 },
  gauge: {
    width: 160, height: 160, borderRadius: 80, borderWidth: 8,
    justifyContent: 'center', alignItems: 'center', backgroundColor: '#fff',
  },
  gaugeScore: { fontSize: 48, fontWeight: 'bold' },
  gaugeLabel: { fontSize: 14, color: '#666', marginTop: 4 },
  gaugeStatus: { fontSize: 18, fontWeight: '600', marginTop: 2 },
  scoreBarRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 6 },
  scoreBarLabel: { width: 90, fontSize: 14 },
  scoreBar: { flex: 1, height: 8, borderRadius: 4 },
  scoreBarValue: { width: 35, textAlign: 'right', fontSize: 14, fontWeight: '600' },
  zoneRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 8 },
  zoneInfo: { flex: 1 },
  zoneName: { fontSize: 16, fontWeight: '600' },
  zoneDetail: { fontSize: 12, color: '#666' },
  zoneBar: { paddingHorizontal: 12, paddingVertical: 8, borderRadius: 8, minWidth: 60 },
  zoneBarText: { color: '#fff', fontWeight: 'bold', textAlign: 'center' },
  alertCard: { borderLeftWidth: 4, marginVertical: 4 },
  alertContent: { flexDirection: 'row', alignItems: 'center' },
  alertText: { flex: 1 },
  alertMessage: { fontSize: 14, fontWeight: '500' },
  alertTime: { fontSize: 12, color: '#888', marginTop: 2 },
  recRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 4 },
  recText: { flex: 1, fontSize: 14 },
  waterRow: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 8 },
  waterCell: { alignItems: 'center' },
  waterValue: { fontSize: 28, fontWeight: 'bold', color: '#1565C0' },
  waterLabel: { fontSize: 12, color: '#666' },
  quickActions: { flexDirection: 'row', justifyContent: 'space-around', margin: 16 },
  actionBtn: { flex: 1, marginHorizontal: 8, backgroundColor: '#2E7D32' },
  runBtn: { marginTop: 8 },
  footer: { textAlign: 'center', fontSize: 12, color: '#999', padding: 16 },
});