// MosquitoSync — React Native Mobile App
//
// AI-powered mosquito detection & disease-risk app.
// Screens: Dashboard, Acoustic, Trap, Barriers, Weather, Forecast,
// Disease Risk, Species Guide, Alerts, Settings.
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

// ─── Species data ──────────────────────────────────────────────────────────
const SPECIES_INFO = [
  { name: 'Aedes aegypti', disease: 'Dengue, Zika, Yellow Fever', freq: '484 Hz' },
  { name: 'Aedes albopictus', disease: 'Dengue, Chikungunya', freq: '428 Hz' },
  { name: 'Anopheles gambiae', disease: 'Malaria', freq: '423 Hz' },
  { name: 'Anopheles stephensi', disease: 'Malaria', freq: '455 Hz' },
  { name: 'Culex quinquefasciatus', disease: 'West Nile, Lymphatic Filariasis', freq: '567 Hz' },
  { name: 'Culex pipiens', disease: 'West Nile', freq: '503 Hz' },
  { name: 'Mansonia uniformis', disease: 'Lymphatic Filariasis', freq: '322 Hz' },
  { name: 'Non-mosquito', disease: '—', freq: '—' },
];

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
  const [biteRisk, setBiteRisk] = useState({ score: 0, level: 'Low', recommendations: [] });
  const [diseaseRisk, setDiseaseRisk] = useState({ score: 0, level: 'Low', dengue_risk: 0, west_nile_risk: 0, malaria_risk: 0 });
  const [species, setSpecies] = useState({});
  const [refreshing, setRefreshing] = useState(false);

  const fetchRisk = useCallback(async () => {
    try {
      const [bite, disease, sp] = await Promise.all([
        fetch(`${API_BASE}/bite-risk`).then(r => r.json()),
        fetch(`${API_BASE}/disease-risk`).then(r => r.json()),
        fetch(`${API_BASE}/species?period=24h`).then(r => r.json()),
      ]);
      setBiteRisk(bite);
      setDiseaseRisk(disease);
      setSpecies(sp.species_counts || {});
    } catch (e) {
      console.log('API error:', e);
    }
  }, []);

  useEffect(() => { fetchRisk(); }, [fetchRisk]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchRisk();
    setRefreshing(false);
  }, [fetchRisk]);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
        <Text style={styles.h1}>MosquitoSync</Text>
        <Text style={styles.subtitle}>Mosquito Intelligence Dashboard</Text>

        {/* Risk Gauges */}
        <View style={styles.row}>
          <RiskGauge score={biteRisk.score} label="Bite Risk" color="#FF9800" />
          <RiskGauge score={diseaseRisk.score} label="Disease Risk" color="#F44336" />
        </View>

        {/* Risk Levels */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Current Risk Levels</Text>
          <Text style={styles.cardRow}>Bite Risk: <Text style={styles.bold}>{biteRisk.level}</Text></Text>
          <Text style={styles.cardRow}>Disease Risk: <Text style={styles.bold}>{diseaseRisk.level}</Text></Text>
          <Text style={styles.cardRow}>Dengue: {(diseaseRisk.dengue_risk * 100).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>West Nile: {(diseaseRisk.west_nile_risk * 100).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>Malaria: {(diseaseRisk.malaria_risk * 100).toFixed(1)}%</Text>
        </View>

        {/* Species Detected */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Species Detected (24h)</Text>
          {Object.keys(species).length === 0 ? (
            <Text style={styles.empty}>No detections yet</Text>
          ) : (
            Object.entries(species).map(([name, count]) => (
              <Text key={name} style={styles.cardRow}>
                {name}: {count}
              </Text>
            ))
          )}
        </View>

        {/* Recommendations */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Recommendations</Text>
          {biteRisk.recommendations?.map((rec, i) => (
            <Text key={i} style={styles.cardRow}>• {rec}</Text>
          ))}
        </View>

        {/* Quick Actions */}
        <TouchableOpacity style={styles.actionButton} onPress={() => {
          fetch(`${API_BASE}/barrier/close`, { method: 'POST' });
          Alert.alert('Barriers', 'Closing all window barriers...');
        }}>
          <Text style={styles.actionText}>🔒 Close All Barriers</Text>
        </TouchableOpacity>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Acoustic Screen ───────────────────────────────────────────────────────
function AcousticScreen() {
  const [detections, setDetections] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/acoustic`)
      .then(r => r.json())
      .then(setDetections)
      .catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Acoustic Sentinels</Text>
        <Text style={styles.subtitle}>Real-time mosquito detection via wingbeat CNN</Text>
        {detections.length === 0 ? (
          <Text style={styles.empty}>No detections yet</Text>
        ) : (
          detections.map((d, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>Node {d.node_id} — {SPECIES_INFO[d.species_class]?.name}</Text>
              <Text style={styles.cardRow}>
                Detected: {d.mosquito_detected ? '✅ Yes' : '❌ No'}
              </Text>
              <Text style={styles.cardRow}>Confidence: {d.confidence_pct}%</Text>
              <Text style={styles.cardRow}>Wingbeat: {d.wingbeat_freq_hz?.toFixed(1)} Hz</Text>
              <Text style={styles.cardRow}>Detections (24h): {d.detections_24h}</Text>
              <Text style={styles.cardRow}>Temp: {d.temp_c}°C, Humidity: {d.humidity_pct}%</Text>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Trap Screen ───────────────────────────────────────────────────────────
function TrapScreen() {
  const [trap, setTrap] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/trap`).then(r => r.json()).then(setTrap).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>CO2 Traps</Text>
        <Text style={styles.subtitle}>Outdoor lure + capture monitoring</Text>
        {trap.map((t, i) => (
          <View key={i} style={styles.card}>
            <Text style={styles.cardTitle}>Trap Node {t.node_id}</Text>
            <Text style={styles.cardRow}>CO2: {t.co2_on ? '✅ On' : '❌ Off'}</Text>
            <Text style={styles.cardRow}>Propane: {t.propane_pct}%</Text>
            <Text style={styles.cardRow}>Fan: {t.fan_pct}%</Text>
            <Text style={styles.cardRow}>Captures (24h): {t.capture_24h}</Text>
            <Text style={styles.cardRow}>Trap Fullness: {t.trap_fullness_pct}%</Text>
            <Text style={styles.cardRow}>Temp: {t.temp_c?.toFixed(1)}°C</Text>
            <Text style={styles.cardRow}>Dominant: {SPECIES_INFO[t.dominant_species]?.name}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Barriers Screen ───────────────────────────────────────────────────────
function BarriersScreen() {
  const [barriers, setBarriers] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/barrier/status`).then(r => r.json()).then(setBarriers).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Window Barriers</Text>
        <Text style={styles.subtitle}>Motorized mosquito screens</Text>
        <View style={styles.row}>
          <TouchableOpacity style={styles.actionButton} onPress={() => {
            fetch(`${API_BASE}/barrier/close`, { method: 'POST' });
            Alert.alert('Closing all barriers...');
          }}>
            <Text style={styles.actionText}>🔒 Close All</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.actionButton} onPress={() => {
            fetch(`${API_BASE}/barrier/open`, { method: 'POST' });
            Alert.alert('Opening all barriers...');
          }}>
            <Text style={styles.actionText}>🔓 Open All</Text>
          </TouchableOpacity>
        </View>
        {barriers.map((b, i) => (
          <View key={i} style={styles.card}>
            <Text style={styles.cardTitle}>Barrier Node {b.node_id}</Text>
            <Text style={styles.cardRow}>
              Status: {b.screen_status === 0 ? '🟢 Open' : b.screen_status === 1 ? '🔴 Closed' : '🟡 Moving'}
            </Text>
            <Text style={styles.cardRow}>Cycles (24h): {b.cycles_24h}</Text>
            <Text style={styles.cardRow}>Battery: {(b.battery_v / 100).toFixed(2)}V</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Disease Risk Screen ────────────────────────────────────────────────────
function DiseaseRiskScreen() {
  const [risk, setRisk] = useState({ dengue_risk: 0, west_nile_risk: 0, malaria_risk: 0, score: 0, level: 'Low' });

  useEffect(() => {
    fetch(`${API_BASE}/disease-risk`).then(r => r.json()).then(setRisk).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Disease Risk</Text>
        <Text style={styles.subtitle}>7-day outbreak forecast</Text>

        <RiskGauge score={risk.score} label="Overall Risk" color="#F44336" />

        <View style={styles.card}>
          <Text style={styles.cardTitle}>Dengue (Aedes aegypti/albopictus)</Text>
          <Text style={styles.cardRow}>Risk: {(risk.dengue_risk * 100).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>Symptoms: High fever, severe headache, joint pain</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>West Nile (Culex spp.)</Text>
          <Text style={styles.cardRow}>Risk: {(risk.west_nile_risk * 100).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>Symptoms: Fever, headache, body aches</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Malaria (Anopheles spp.)</Text>
          <Text style={styles.cardRow}>Risk: {(risk.malaria_risk * 100).toFixed(1)}%</Text>
          <Text style={styles.cardRow}>Symptoms: Chills, fever, sweats</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Species Guide Screen ──────────────────────────────────────────────────
function SpeciesGuideScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Species Guide</Text>
        <Text style={styles.subtitle}>Mosquito species & diseases</Text>
        {SPECIES_INFO.map((s, i) => (
          <View key={i} style={styles.card}>
            <Text style={styles.cardTitle}>{s.name}</Text>
            <Text style={styles.cardRow}>Wingbeat: {s.freq}</Text>
            <Text style={styles.cardRow}>Diseases: {s.disease}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Alerts Screen ──────────────────────────────────────────────────────────
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
          <Text style={styles.cardRow}>Manage connected MosquitoSync nodes</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Notifications</Text>
          <Text style={styles.cardRow}>Disease vector detected</Text>
          <Text style={styles.cardRow}>High disease risk</Text>
          <Text style={styles.cardRow}>Trap full</Text>
          <Text style={styles.cardRow}>Barrier auto-closed</Text>
          <Text style={styles.cardRow}>Battery low</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Personal Profile</Text>
          <Text style={styles.cardRow}>Blood type (affects BiteRisk)</Text>
          <Text style={styles.cardRow}>Pregnancy status</Text>
          <Text style={styles.cardRow}>CO2 emission estimate</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Emergency Contacts</Text>
          <Text style={styles.cardRow}>Set contacts for critical alerts</Text>
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
        <Tab.Screen name="Acoustic" component={AcousticScreen} />
        <Tab.Screen name="Trap" component={TrapScreen} />
        <Tab.Screen name="Barriers" component={BarriersScreen} />
        <Tab.Screen name="Disease" component={DiseaseRiskScreen} />
        <Tab.Screen name="Species" component={SpeciesGuideScreen} />
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
  actionButton: {
    backgroundColor: '#1a3a5a', borderRadius: 8, padding: 14,
    marginHorizontal: 8, marginBottom: 12, alignItems: 'center', flex: 1,
  },
  actionText: { color: '#4fc3f7', fontSize: 14, fontWeight: 'bold' },
});