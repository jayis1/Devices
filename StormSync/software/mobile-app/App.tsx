/**
 * StormSync Mobile App — Dashboard Screen
 *
 * Main screen showing StormSync Score, sump pit water level gauge,
 * pump status, flood forecast chart, and active alerts.
 *
 * React Native + Expo + React Native Paper
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  View, Text, StyleSheet, ScrollView, RefreshControl,
  TouchableOpacity, Alert, Dimensions
} from 'react-native';
import { Card, Title, Paragraph, ProgressBar, Button, IconButton,
  ActivityIndicator, Divider, Surface, Switch } from 'react-native-paper';
import { LineChart } from 'react-native-chart-kit';

const API_BASE = 'https://api.stormsync.cloud/api/v1';
const screenWidth = Dimensions.get('window').width;

// ---- Types ----
interface FloodScore {
  score: number;
  risk_level: string;
  confidence: number;
  factors: Record<string, string>;
  recommendations: string[];
}

interface SumpReading {
  water_level_mm: number;
  water_level_pct: number;
  pump_status: string;
  pump_current_ma: number;
  vibration_rms_mg: number;
  mains_power: boolean;
  battery_v: number;
}

interface ActuatorStatus {
  valve_status: string;
  pump_relay: boolean;
  float_switch: boolean;
  alarm_status: boolean;
  mains_power: boolean;
  battery_v: number;
  battery_health_pct: number;
}

interface AlertItem {
  id: string;
  node_id: number;
  alert_type: string;
  severity: number;
  message: string;
  timestamp: string;
  acknowledged: boolean;
}

interface PumpHealth {
  classification: string;
  confidence: number;
  predicted_time_to_failure_days: number | null;
  vibration_trend: string;
  maintenance_recommended: boolean;
  notes: string;
}

// ---- Helpers ----
const scoreColor = (score: number): string => {
  if (score <= 30) return '#2E7D32';   // green
  if (score <= 55) return '#F9A825';   // amber
  if (score <= 75) return '#EF6C00';   // orange
  return '#C62828';                     // red
};

const riskLabel = (level: string): string => {
  const labels: Record<string, string> = {
    low: 'Low Risk', moderate: 'Moderate Risk',
    high: 'High Risk', critical: 'CRITICAL',
  };
  return labels[level] || level;
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

const pumpStatusColor = (status: string): string => {
  if (status === 'running') return '#4CAF50';
  if (status === 'fault') return '#F44336';
  return '#757575';
};

// ---- FloodScoreGauge ----
const FloodScoreGauge: React.FC<{ score: number; riskLevel: string }> = ({ score, riskLevel }) => {
  const color = scoreColor(score);
  return (
    <View style={styles.gaugeContainer}>
      <Surface style={[styles.gauge, { borderColor: color }]} elevation={4}>
        <Text style={[styles.gaugeScore, { color }]}>{score}</Text>
        <Text style={styles.gaugeLabel}>StormSync Score</Text>
        <Text style={[styles.gaugeStatus, { color }]}>{riskLabel(riskLevel)}</Text>
      </Surface>
    </View>
  );
};

// ---- SumpLevelGauge ----
const SumpLevelGauge: React.FC<{ reading: SumpReading }> = ({ reading }) => {
  const pct = reading.water_level_pct;
  const color = pct > 85 ? '#C62828' : pct > 70 ? '#EF6C00' : pct > 40 ? '#F9A825' : '#2E7D32';
  return (
    <Card style={styles.card}>
      <Card.Content>
        <Title>Sump Pit Water Level</Title>
        <View style={styles.levelContainer}>
          <View style={styles.levelBar}>
            <View style={[styles.levelFill, {
              height: `${pct}%`, backgroundColor: color
            }]} />
          </View>
          <View style={styles.levelInfo}>
            <Text style={[styles.levelValue, { color }]}>{pct.toFixed(0)}%</Text>
            <Text style={styles.levelDetail}>{reading.water_level_mm} mm</Text>
            <Text style={[styles.pumpStatus, { color: pumpStatusColor(reading.pump_status) }]}>
              Pump: {reading.pump_status}
            </Text>
            <Text style={styles.levelDetail}>Vibration: {reading.vibration_rms_mg} mg</Text>
            <Text style={styles.levelDetail}>
              Power: {reading.mains_power ? 'Mains OK' : '⚠️ BATTERY'}
            </Text>
            <Text style={styles.levelDetail}>Battery: {reading.battery_v.toFixed(1)}V</Text>
          </View>
        </View>
      </Card.Content>
    </Card>
  );
};

// ---- AlertCard ----
const AlertCard: React.FC<{ alert: AlertItem; onPress: (id: string) => void }> = ({ alert, onPress }) => (
  <Card style={[styles.alertCard, { borderLeftColor: severityColor(alert.severity) }]}>
    <Card.Content style={styles.alertContent}>
      <IconButton icon={severityIcon(alert.severity)} size={24}
        iconColor={severityColor(alert.severity)} />
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

// ---- ActuatorControl ----
const ActuatorControl: React.FC<{ status: ActuatorStatus }> = ({ status }) => {
  const [valveOpen, setValveOpen] = useState(status.valve_status === 'open');
  const [pumpOn, setPumpOn] = useState(status.pump_relay);

  const toggleValve = async () => {
    const action = valveOpen ? 'close' : 'open';
    Alert.alert(
      'Confirm',
      `${action === 'close' ? 'Close' : 'Open'} backflow preventer valve?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Confirm', onPress: async () => {
            await fetch(`${API_BASE}/actuator/valve`, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ action }),
            });
            setValveOpen(!valveOpen);
          }
        }
      ]
    );
  };

  const togglePump = async () => {
    const action = pumpOn ? 'off' : 'on';
    Alert.alert(
      'Confirm',
      `${action === 'on' ? 'Start' : 'Stop'} backup pump?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Confirm', onPress: async () => {
            await fetch(`${API_BASE}/actuator/pump`, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ action }),
            });
            setPumpOn(!pumpOn);
          }
        }
      ]
    );
  };

  return (
    <Card style={styles.card}>
      <Card.Content>
        <Title>Flood Defense</Title>
        <View style={styles.controlRow}>
          <Text style={styles.controlLabel}>Backflow Valve</Text>
          <Switch value={valveOpen} onValueChange={toggleValve}
            color={valveOpen ? '#4CAF50' : '#F44336'} />
          <Text style={styles.controlStatus}>
            {valveOpen ? 'Open' : 'Closed'}
          </Text>
        </View>
        <View style={styles.controlRow}>
          <Text style={styles.controlLabel}>Backup Pump</Text>
          <Switch value={pumpOn} onValueChange={togglePump}
            color={pumpOn ? '#4CAF50' : '#757575'} />
          <Text style={styles.controlStatus}>
            {pumpOn ? 'Running' : 'Off'}
          </Text>
        </View>
        <Divider style={styles.divider} />
        <View style={styles.batteryRow}>
          <Text style={styles.controlLabel}>Battery: {status.battery_v.toFixed(1)}V ({status.battery_health_pct}%)</Text>
          <Text style={styles.controlLabel}>
            Float: {status.float_switch ? '⚠️ HIGH' : 'Normal'}
          </Text>
        </View>
      </Card.Content>
    </Card>
  );
};

// ---- Main Dashboard ----
const DashboardScreen: React.FC<{ navigation: any }> = ({ navigation }) => {
  const [score, setScore] = useState<FloodScore | null>(null);
  const [sump, setSump] = useState<SumpReading | null>(null);
  const [actuator, setActuator] = useState<ActuatorStatus | null>(null);
  const [alerts, setAlerts] = useState<AlertItem[]>([]);
  const [pumpHealth, setPumpHealth] = useState<PumpHealth | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAll = useCallback(async () => {
    try {
      const [scoreR, sumpR, actR, alertR, pumpR] = await Promise.all([
        fetch(`${API_BASE}/flood-score`).then(r => r.json()),
        fetch(`${API_BASE}/sump`).then(r => r.json()),
        fetch(`${API_BASE}/actuator/status`).then(r => r.json()),
        fetch(`${API_BASE}/alerts?acknowledged=false`).then(r => r.json()),
        fetch(`${API_BASE}/pump-health`).then(r => r.json()),
      ]);
      setScore(scoreR);
      setSump(sumpR);
      setActuator(actR);
      setAlerts(alertR);
      setPumpHealth(pumpR);
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
        <ActivityIndicator size="large" color="#1565C0" />
        <Text style={styles.loadingText}>Loading flood status...</Text>
      </View>
    );
  }

  return (
    <ScrollView
      style={styles.container}
      refreshControl={
        <RefreshControl refreshing={refreshing}
          onRefresh={() => { setRefreshing(true); fetchAll(); }}
          colors={['#1565C0']} />
      }
    >
      {/* Flood Score Gauge */}
      {score && (
        <>
          <FloodScoreGauge score={score.score} riskLevel={score.risk_level} />

          {/* Recommendations */}
          <Card style={styles.card}>
            <Card.Content>
              <Title>Recommendations</Title>
              {score.recommendations.map((rec, i) => (
                <View key={i} style={styles.recRow}>
                  <IconButton icon="alert-circle-outline" size={20}
                    iconColor={scoreColor(score.score)} />
                  <Text style={styles.recText}>{rec}</Text>
                </View>
              ))}
            </Card.Content>
          </Card>
        </>
      )}

      {/* Sump Pit Level */}
      {sump && <SumpLevelGauge reading={sump} />}

      {/* Pump Health */}
      {pumpHealth && (
        <Card style={styles.card}>
          <Card.Content>
            <Title>Pump Health: {pumpHealth.classification.replace(/_/g, ' ')}</Title>
            <Text style={styles.pumpHealthConfidence}>
              Confidence: {(pumpHealth.confidence * 100).toFixed(0)}%
            </Text>
            <Text style={styles.pumpHealthDetail}>
              Vibration trend: {pumpHealth.vibration_trend}
            </Text>
            {pumpHealth.predicted_time_to_failure_days && (
              <Text style={[styles.pumpHealthWarning, { color: '#FF9800' }]}>
                ⚠️ Estimated {pumpHealth.predicted_time_to_failure_days} days to failure
              </Text>
            )}
            <Text style={styles.pumpHealthNotes}>{pumpHealth.notes}</Text>
          </Card.Content>
        </Card>
      )}

      {/* Flood Defense Controls */}
      {actuator && <ActuatorControl status={actuator} />}

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

      {/* Quick Actions */}
      <View style={styles.quickActions}>
        <Button mode="contained" icon="chart-line"
          onPress={() => navigation.navigate('Forecast')}
          style={styles.actionBtn} color="#1565C0">
          Forecast
        </Button>
        <Button mode="contained" icon="water"
          onPress={() => navigation.navigate('Soil')}
          style={styles.actionBtn} color="#1565C0">
          Soil
        </Button>
      </View>

      <Divider />
      <Text style={styles.footer}>StormSync v1.0 • Flood Protection AI</Text>
    </ScrollView>
  );
};

// ---- Forecast Screen ----
const ForecastScreen: React.FC = () => {
  const [forecast, setForecast] = useState<any>(null);

  useEffect(() => {
    fetch(`${API_BASE}/flood-forecast`).then(r => r.json()).then(setForecast);
  }, []);

  if (!forecast) return <ActivityIndicator style={styles.centered} size="large" color="#1565C0" />;

  const chartData = {
    labels: forecast.predictions.filter((_: any, i: number) => i % 4 === 0)
      .map((p: any) => new Date(p.timestamp).toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'})),
    datasets: [{
      data: forecast.predictions.map((p: any) => p.water_level_mm),
      color: (opacity = 1) => `rgba(21, 101, 192, ${opacity})`,
      strokeWidth: 2,
    }],
  };

  return (
    <ScrollView style={styles.container}>
      <Title style={styles.screenTitle}>6-Hour Flood Forecast</Title>
      <Card style={styles.card}>
        <Card.Content>
          <Paragraph>Max predicted: {forecast.max_predicted_level_mm} mm</Paragraph>
          <Paragraph>Flood threshold: {forecast.flood_threshold_mm} mm</Paragraph>
          <Paragraph style={{ color: forecast.flood_predicted ? '#F44336' : '#4CAF50', fontWeight: 'bold' }}>
            {forecast.flood_predicted ? '⚠️ FLOOD PREDICTED' : '✓ No flood predicted'}
          </Paragraph>
        </Card.Content>
      </Card>
      <LineChart
        data={chartData}
        width={screenWidth - 32}
        height={220}
        chartConfig={{
          backgroundGradientFrom: '#fff',
          backgroundGradientTo: '#fff',
          color: (opacity = 1) => `rgba(21, 101, 192, ${opacity})`,
          labelColor: (opacity = 1) => `rgba(0, 0, 0, ${opacity})`,
        }}
        bezier
        style={{ margin: 16, borderRadius: 12 }}
      />
    </ScrollView>
  );
};

// ---- Soil Screen ----
const SoilScreen: React.FC = () => {
  const [soil, setSoil] = useState<any[]>([]);

  useEffect(() => {
    fetch(`${API_BASE}/soil`).then(r => r.json()).then(setSoil);
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Title style={styles.screenTitle}>Soil Saturation</Title>
      {soil.map((s, i) => (
        <Card key={i} style={styles.card}>
          <Card.Content>
            <Title>Probe {s.node_id - 1}</Title>
            <View style={styles.soilDepth}>
              <Text style={styles.soilLabel}>15cm:</Text>
              <ProgressBar progress={s.moisture_15_pct / 100}
                color={s.moisture_15_pct > 85 ? '#F44336' : '#4CAF50'}
                style={styles.soilBar} />
              <Text style={styles.soilValue}>{s.moisture_15_pct.toFixed(0)}%</Text>
            </View>
            <View style={styles.soilDepth}>
              <Text style={styles.soilLabel}>45cm:</Text>
              <ProgressBar progress={s.moisture_45_pct / 100}
                color={s.moisture_45_pct > 85 ? '#F44336' : '#4CAF50'}
                style={styles.soilBar} />
              <Text style={styles.soilValue}>{s.moisture_45_pct.toFixed(0)}%</Text>
            </View>
            <View style={styles.soilDepth}>
              <Text style={styles.soilLabel}>90cm:</Text>
              <ProgressBar progress={s.moisture_90_pct / 100}
                color={s.moisture_90_pct > 85 ? '#F44336' : '#4CAF50'}
                style={styles.soilBar} />
              <Text style={styles.soilValue}>{s.moisture_90_pct.toFixed(0)}%</Text>
            </View>
            <Paragraph style={styles.soilPore}>
              Pore pressure: {s.pore_pressure_kpa.toFixed(1)} kPa
            </Paragraph>
          </Card.Content>
        </Card>
      ))}
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
            tabBarActiveTintColor: '#1565C0',
            headerStyle: { backgroundColor: '#1565C0' },
            headerTintColor: '#fff',
          }}
        >
          <Tab.Screen name="Dashboard" component={DashboardScreen}
            options={{ tabBarIcon: 'home' }} />
          <Tab.Screen name="Forecast" component={ForecastScreen}
            options={{ tabBarIcon: 'chart-line' }} />
          <Tab.Screen name="Soil" component={SoilScreen}
            options={{ tabBarIcon: 'water' }} />
        </Tab.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}

// ---- Styles ----
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA', padding: 8 },
  centered: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  loadingText: { marginTop: 10, fontSize: 16, color: '#1565C0' },
  screenTitle: { fontSize: 24, fontWeight: 'bold', margin: 12, color: '#1565C0' },
  card: { marginBottom: 12, borderRadius: 12 },
  gaugeContainer: { alignItems: 'center', padding: 20 },
  gauge: {
    width: 180, height: 180, borderRadius: 90, borderWidth: 8,
    justifyContent: 'center', alignItems: 'center', backgroundColor: '#fff',
  },
  gaugeScore: { fontSize: 56, fontWeight: 'bold' },
  gaugeLabel: { fontSize: 14, color: '#666', marginTop: 4 },
  gaugeStatus: { fontSize: 18, fontWeight: '600', marginTop: 2 },
  levelContainer: { flexDirection: 'row', marginTop: 12 },
  levelBar: {
    width: 40, height: 120, borderRadius: 8, backgroundColor: '#E0E0E0',
    marginRight: 20, overflow: 'hidden', justifyContent: 'flex-end',
  },
  levelFill: { width: '100%', borderRadius: 8 },
  levelInfo: { flex: 1, justifyContent: 'center' },
  levelValue: { fontSize: 32, fontWeight: 'bold' },
  levelDetail: { fontSize: 13, color: '#666', marginTop: 2 },
  pumpStatus: { fontSize: 14, fontWeight: '600', marginTop: 6 },
  alertCard: { borderLeftWidth: 4, marginVertical: 4 },
  alertContent: { flexDirection: 'row', alignItems: 'center' },
  alertText: { flex: 1 },
  alertMessage: { fontSize: 14, fontWeight: '500' },
  alertTime: { fontSize: 12, color: '#888', marginTop: 2 },
  recRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 4 },
  recText: { flex: 1, fontSize: 14 },
  controlRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 8 },
  controlLabel: { flex: 1, fontSize: 14, fontWeight: '500' },
  controlStatus: { fontSize: 14, marginLeft: 8, fontWeight: '600' },
  batteryRow: { flexDirection: 'row', justifyContent: 'space-between', marginTop: 4 },
  divider: { marginVertical: 8 },
  pumpHealthConfidence: { fontSize: 14, color: '#666', marginTop: 4 },
  pumpHealthDetail: { fontSize: 13, color: '#666', marginTop: 4 },
  pumpHealthWarning: { fontSize: 15, fontWeight: '600', marginTop: 6 },
  pumpHealthNotes: { fontSize: 12, color: '#888', marginTop: 8 },
  soilDepth: { flexDirection: 'row', alignItems: 'center', marginVertical: 4 },
  soilLabel: { width: 55, fontSize: 14 },
  soilBar: { flex: 1, height: 10, borderRadius: 5, marginHorizontal: 8 },
  soilValue: { width: 40, textAlign: 'right', fontSize: 14, fontWeight: '600' },
  soilPore: { fontSize: 13, color: '#666', marginTop: 8 },
  quickActions: { flexDirection: 'row', justifyContent: 'space-around', margin: 16 },
  actionBtn: { flex: 1, marginHorizontal: 8 },
  footer: { textAlign: 'center', fontSize: 12, color: '#999', padding: 16 },
});