// VoiceSync — React Native Mobile App
//
// AI-powered voice health & vocal wellness app.
// Screens: Dashboard, Vocal Band, Room Sentinel, Hydration, Risk Forecast,
// Voice Guide, Alerts, Settings.
//
// Build: react-native run-android / run-ios

import React, { useState, useEffect, useCallback } from 'react';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity,
  Alert, RefreshControl, SafeAreaView, Dimensions,
} from 'react-native';
import {
  NavigationContainer, useNavigation,
} from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';

const { width } = Dimensions.get('window');
const API_BASE = 'http://localhost:8080/api/v1';

// ─── Voice quality classes ──────────────────────────────────────────────────
const VOICE_QUALITY = [
  { name: 'Normal', color: '#4CAF50', desc: 'Clear, resonant voice' },
  { name: 'Hoarse', color: '#FF9800', desc: 'Rough, breathy quality' },
  { name: 'Breathy', color: '#FF9800', desc: 'Excessive air escape' },
  { name: 'Strained', color: '#FF9800', desc: 'Effortful, tense voice' },
  { name: 'Tremor', color: '#F44336', desc: 'Involuntary pitch wavering' },
  { name: 'Fatigue', color: '#FF9800', desc: 'Reduced volume + clarity' },
  { name: 'Reflux', color: '#F44336', desc: 'Acid-damaged vocal quality' },
  { name: 'Disorder', color: '#F44336', desc: 'Severe quality degradation' },
];

const CLINICAL_THRESHOLDS = {
  jitter: { normal: 1.04, mild: 2.61, moderate: 4.52 },
  shimmer: { normal: 3.81, mild: 7.62, moderate: 11.4 },
  hnr: { normal: 20, mild: 15, moderate: 10 },
};

// ─── Risk gauge component ───────────────────────────────────────────────────
function RiskGauge({ score, label, color }: { score: number; label: string; color: string }) {
  const size = 120;
  const strokeWidth = 12;
  const radius = (size - strokeWidth) / 2;
  const circumference = 2 * Math.PI * radius;
  const progress = score / 100;
  const dashOffset = circumference * (1 - progress);

  return (
    <View style={styles.gaugeContainer}>
      <View style={[styles.gauge, { width: size, height: size }]}>
        <Text style={styles.gaugeScore}>{score}</Text>
        <Text style={styles.gaugeLabel}>{label}</Text>
      </View>
      <Text style={[styles.gaugeTitle, { color }]}>{label}</Text>
    </View>
  );
}

// ─── Dashboard Screen ──────────────────────────────────────────────────────
function DashboardScreen() {
  const [vocalHealth, setVocalHealth] = useState({ score: 0, level: 'Good', f0_hz: 0, jitter_pct: 0, shimmer_pct: 0, hnr_db: 0, phonation_pct: 0, recommendations: [] });
  const [disorderRisk, setDisorderRisk] = useState({ score: 0, level: 'Low', nodules_risk: 0, reflux_risk: 0, fatigue_risk: 0 });
  const [vocalLoad, setVocalLoad] = useState({ phonation_pct: 0, safe_dose_pct: 30, rest_recommended: false });
  const [hydration, setHydration] = useState({ intake_ml: 0 });
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const [health, risk, load, hyd] = await Promise.all([
        fetch(`${API_BASE}/vocal-health`).then(r => r.json()),
        fetch(`${API_BASE}/voice-disorder-risk`).then(r => r.json()),
        fetch(`${API_BASE}/vocal-load`).then(r => r.json()),
        fetch(`${API_BASE}/hydration`).then(r => r.json()),
      ]);
      setVocalHealth(health);
      setDisorderRisk(risk);
      setVocalLoad(load);
      setHydration(hyd[0] || { intake_ml: 0 });
    } catch (e) {
      console.log('API error:', e);
    }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  }, [fetchData]);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
        <Text style={styles.h1}>VoiceSync</Text>
        <Text style={styles.subtitle}>Voice Health Dashboard</Text>

        {/* Health Gauges */}
        <View style={styles.row}>
          <RiskGauge score={vocalHealth.score} label="Vocal Health" color="#4CAF50" />
          <RiskGauge score={disorderRisk.score} label="Disorder Risk" color="#F44336" />
        </View>

        {/* Current Status */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Current Status</Text>
          <Text style={styles.cardRow}>Vocal Health: <Text style={styles.bold}>{vocalHealth.level}</Text></Text>
          <Text style={styles.cardRow}>Disorder Risk: <Text style={styles.bold}>{disorderRisk.level}</Text></Text>
          <Text style={styles.cardRow}>F0: {vocalHealth.f0_hz?.toFixed(1)} Hz</Text>
          <Text style={styles.cardRow}>Jitter: {vocalHealth.jitter_pct?.toFixed(2)}%</Text>
          <Text style={styles.cardRow}>Shimmer: {vocalHealth.shimmer_pct?.toFixed(2)}%</Text>
          <Text style={styles.cardRow}>HNR: {vocalHealth.hnr_db?.toFixed(1)} dB</Text>
        </View>

        {/* Vocal Load */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Today's Vocal Load</Text>
          <Text style={styles.cardRow}>Phonation: {vocalLoad.phonation_pct}% of waking hours</Text>
          <Text style={styles.cardRow}>Safe dose: {vocalLoad.safe_dose_pct}% (NCVS)</Text>
          <Text style={styles.cardRow}>Rest: {vocalLoad.rest_recommended ? '⚠️ Recommended' : '✅ Not needed'}</Text>
        </View>

        {/* Hydration */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Hydration</Text>
          <Text style={styles.cardRow}>Today: {hydration.intake_ml} mL / 2000 mL</Text>
          <Text style={styles.cardRow}>{Math.min(100, Math.round(hydration.intake_ml / 20))}% of daily target</Text>
        </View>

        {/* Recommendations */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Recommendations</Text>
          {vocalHealth.recommendations?.map((rec, i) => (
            <Text key={i} style={styles.cardRow}>• {rec}</Text>
          ))}
        </View>

        {/* Rest button */}
        {vocalLoad.rest_recommended && (
          <TouchableOpacity style={styles.alertButton} onPress={() => {
            Alert.alert('Vocal Rest', 'Take a 10-minute vocal break. Avoid speaking, drink water, and relax your neck muscles.');
          }}>
            <Text style={styles.actionText}>⏸️ Vocal Rest Needed (10 min)</Text>
          </TouchableOpacity>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Vocal Band Screen ─────────────────────────────────────────────────────
function VocalBandScreen() {
  const [readings, setReadings] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/vocal-band`)
      .then(r => r.json())
      .then(setReadings)
      .catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Vocal Band</Text>
        <Text style={styles.subtitle}>Throat contact mic — vocal fold vibration analysis</Text>
        {readings.length === 0 ? (
          <Text style={styles.empty}>No readings yet</Text>
        ) : (
          readings.map((r, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>Node {r.node_id}</Text>
              <Text style={styles.cardRow}>F0: {r.f0_hz?.toFixed(1)} Hz</Text>
              <Text style={styles.cardRow}>Jitter: {r.jitter_pct?.toFixed(2)}% {r.jitter_pct < 1.04 ? '✅' : '⚠️'}</Text>
              <Text style={styles.cardRow}>Shimmer: {r.shimmer_pct?.toFixed(2)}% {r.shimmer_pct < 3.81 ? '✅' : '⚠️'}</Text>
              <Text style={styles.cardRow}>HNR: {r.hnr_db?.toFixed(1)} dB {r.hnr_db > 20 ? '✅' : '⚠️'}</Text>
              <Text style={styles.cardRow}>Phonation: {r.phonation_pct}%</Text>
              <Text style={styles.cardRow}>Skin Temp: {r.skin_temp_c?.toFixed(1)}°C</Text>
              <Text style={styles.cardRow}>HR: {r.heart_rate} bpm, HRV: {r.hrv_rmssd} ms</Text>
              <Text style={styles.cardRow}>Stress: {r.stress_level}/100</Text>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Room Sentinel Screen ─────────────────────────────────────────────────
function RoomSentinelScreen() {
  const [readings, setReadings] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/room-sentinel`)
      .then(r => r.json())
      .then(setReadings)
      .catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Room Sentinel</Text>
        <Text style={styles.subtitle}>Ambient voice quality monitoring (VoiceNet CNN)</Text>
        {readings.length === 0 ? (
          <Text style={styles.empty}>No readings yet</Text>
        ) : (
          readings.map((r, i) => {
            const vq = VOICE_QUALITY[r.voice_quality_class] || VOICE_QUALITY[0];
            return (
              <View key={i} style={styles.card}>
                <Text style={[styles.cardTitle, { color: vq.color }]}>
                  {vq.name} ({r.confidence_pct}%)
                </Text>
                <Text style={styles.cardRow}>{vq.desc}</Text>
                <Text style={styles.cardRow}>F0: {r.f0_hz?.toFixed(1)} Hz</Text>
                <Text style={styles.cardRow}>Temp: {r.temp_c?.toFixed(1)}°C, Humidity: {r.humidity_pct?.toFixed(1)}%</Text>
                <Text style={styles.cardRow}>VOC: {r.voc_index}</Text>
                <Text style={styles.cardRow}>dB SPL: {r.db_spl}</Text>
                <Text style={styles.cardRow}>Talking: {r.talking_detected ? 'Yes' : 'No'}</Text>
              </View>
            );
          })
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Hydration Screen ─────────────────────────────────────────────────────
function HydrationScreen() {
  const [readings, setReadings] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/hydration`)
      .then(r => r.json())
      .then(setReadings)
      .catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Hydration</Text>
        <Text style={styles.subtitle}>Water intake tracking (6-month CR2032)</Text>
        {readings.map((r, i) => (
          <View key={i} style={styles.card}>
            <Text style={styles.cardTitle}>Bottle {r.node_id}</Text>
            <Text style={styles.cardRow}>Mass: {r.water_mass_g} g</Text>
            <Text style={styles.cardRow}>Sips (24h): {r.sips_24h}</Text>
            <Text style={styles.cardRow}>Intake: {r.intake_ml} mL / 2000 mL</Text>
            <Text style={styles.cardRow}>Last sip: {r.last_sip_min} min ago</Text>
            <Text style={styles.cardRow}>Battery: {(r.battery_v / 100).toFixed(2)}V</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Risk Forecast Screen ─────────────────────────────────────────────────
function RiskForecastScreen() {
  const [forecast, setForecast] = useState({ risk_index: [], timestamps: [] });

  useEffect(() => {
    fetch(`${API_BASE}/ml/predict/risk`)
      .then(r => r.json())
      .then(setForecast)
      .catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>7-Day Risk Forecast</Text>
        <Text style={styles.subtitle}>Voice disorder risk LSTM (168-hour)</Text>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Risk Timeline</Text>
          {forecast.risk_index.length > 0 ? (
            forecast.risk_index.slice(0, 24).map((risk, i) => (
              <Text key={i} style={styles.cardRow}>
                Hour {i}: {Math.round(risk * 100)}% {risk > 0.5 ? '⚠️' : '✅'}
              </Text>
            ))
          ) : (
            <Text style={styles.empty}>Loading forecast...</Text>
          )}
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Disorder Risk Breakdown</Text>
          <Text style={styles.cardRow}>Nodules: {(forecast.nodules_risk * 100 || 0).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>Reflux (LPR): {(forecast.reflux_risk * 100 || 0).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>Fatigue: {(forecast.fatigue_risk * 100 || 0).toFixed(1)}%</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Voice Guide Screen ───────────────────────────────────────────────────
function VoiceGuideScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Voice Guide</Text>
        <Text style={styles.subtitle}>Vocal hygiene & exercises</Text>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>💧 Hydration</Text>
          <Text style={styles.cardRow}>• Drink 35 mL/kg/day water</Text>
          <Text style={styles.cardRow}>• Sip throughout the day, not all at once</Text>
          <Text style={styles.cardRow}>• Avoid caffeine/alcohol (dehydrating)</Text>
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>⏸️ Vocal Rest</Text>
          <Text style={styles.cardRow}>• NCVS safe dose: {'<'}30% phonation</Text>
          <Text style={styles.cardRow}>• Break every 5 minutes of continuous speech</Text>
          <Text style={styles.cardRow}>• 10-minute rest after heavy use</Text>
          <Text style={styles.cardRow}>• Avoid throat clearing</Text>
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>🌬️ Warm-Up Exercises</Text>
          <Text style={styles.cardRow}>• Lip trills (1 min)</Text>
          <Text style={styles.cardRow}>• Humming (1 min, low to high pitch)</Text>
          <Text style={styles.cardRow}>• Straw phonation (1 min)</Text>
          <Text style={styles.cardRow}>• Tongue trills (1 min)</Text>
          <Text style={styles.cardRow}>• Gentle glides (low to high pitch, 1 min)</Text>
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>🧍 Posture</Text>
          <Text style={styles.cardRow}>• Keep neck aligned with spine</Text>
          <Text style={styles.cardRow}>• Avoid forward head posture</Text>
          <Text style={styles.cardRow}>• Relax jaw and shoulders</Text>
          <Text style={styles.cardRow}>• Sit/stand tall for optimal breath support</Text>
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>🌡️ Environment</Text>
          <Text style={styles.cardRow}>• Maintain 40-60% humidity</Text>
          <Text style={styles.cardRow}>• Avoid dusty/dry environments</Text>
          <Text style={styles.cardRow}>• Use humidifier in winter/dry climates</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Alerts Screen ─────────────────────────────────────────────────────────
function AlertsScreen() {
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/alerts`).then(r => r.json()).then(setAlerts).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Alerts</Text>
        {alerts.length === 0 ? (
          <Text style={styles.empty}>No alerts</Text>
        ) : (
          alerts.map((a, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>
                {a.severity === 'critical' ? '🔴' : '🟡'} {a.type}
              </Text>
              <Text style={styles.cardRow}>{a.message}</Text>
              <Text style={styles.cardRow}>Time: {a.timestamp}</Text>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Settings Screen ───────────────────────────────────────────────────────
function SettingsScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Settings</Text>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Devices</Text>
          <Text style={styles.cardRow}>Manage connected VoiceSync nodes</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Notifications</Text>
          <Text style={styles.cardRow}>Vocal rest needed</Text>
          <Text style={styles.cardRow}>High voice disorder risk</Text>
          <Text style={styles.cardRow}>Hoarseness detected</Text>
          <Text style={styles.cardRow}>Reflux pattern detected</Text>
          <Text style={styles.cardRow}>Low humidity</Text>
          <Text style={styles.cardRow}>Dehydration reminder</Text>
          <Text style={styles.cardRow}>Battery low</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Professional Profile</Text>
          <Text style={styles.cardRow}>Voice type (Soprano, Alto, Tenor, Bass)</Text>
          <Text style={styles.cardRow}>Profession (Teacher, Singer, Speaker...)</Text>
          <Text style={styles.cardRow}>Daily speaking hours</Text>
          <Text style={styles.cardRow}>Body weight (hydration target)</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Clinical Reports</Text>
          <Text style={styles.cardRow}>Generate speech-pathologist report</Text>
          <Text style={styles.cardRow}>Export clinical data (PDF)</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Navigation ────────────────────────────────────────────────────────────
const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator screenOptions={{ headerShown: false }}>
        <Tab.Screen name="Dashboard" component={DashboardScreen} />
        <Tab.Screen name="Vocal Band" component={VocalBandScreen} />
        <Tab.Screen name="Room" component={RoomSentinelScreen} />
        <Tab.Screen name="Hydration" component={HydrationScreen} />
        <Tab.Screen name="Risk" component={RiskForecastScreen} />
        <Tab.Screen name="Guide" component={VoiceGuideScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
        <Tab.Screen name="Settings" component={SettingsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

// ─── Styles ────────────────────────────────────────────────────────────────
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a0e14' },
  h1: { fontSize: 28, fontWeight: 'bold', color: '#fff', padding: 16 },
  subtitle: { fontSize: 14, color: '#8899aa', paddingHorizontal: 16, marginBottom: 8 },
  card: {
    backgroundColor: '#1a2330', borderRadius: 12, padding: 16,
    marginHorizontal: 16, marginBottom: 12,
  },
  cardTitle: { fontSize: 16, fontWeight: 'bold', color: '#4fc3f7', marginBottom: 8 },
  cardRow: { fontSize: 14, color: '#cfd8dc', marginBottom: 4 },
  empty: { fontSize: 14, color: '#8899aa', padding: 16, textAlign: 'center' },
  bold: { fontWeight: 'bold', color: '#fff' },
  row: { flexDirection: 'row', justifyContent: 'space-around', paddingHorizontal: 16, marginBottom: 16 },
  gaugeContainer: { alignItems: 'center' },
  gauge: {
    borderRadius: 60, borderWidth: 12, borderColor: '#333',
    justifyContent: 'center', alignItems: 'center',
  },
  gaugeScore: { fontSize: 32, fontWeight: 'bold', color: '#fff' },
  gaugeLabel: { fontSize: 12, color: '#8899aa' },
  gaugeTitle: { fontSize: 14, fontWeight: 'bold', marginTop: 8 },
  alertButton: {
    backgroundColor: '#3a1a1a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  actionText: { color: '#ff6b6b', fontSize: 14, fontWeight: 'bold' },
});