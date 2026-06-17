/**
 * App.tsx — WashWise Mobile App (React Native)
 *
 * Navigation: Home → LaundryStatus, FireSafety, ScanHistory, EnergyReport, SetupWizard
 *
 * Fire safety is the headline feature — fire risk gauge always visible.
 */

import React, {useState, useEffect} from 'react';
import {NavigationContainer} from '@react-navigation/native';
import {createBottomTabNavigator} from '@react-navigation/bottom-tabs';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity,
  SafeAreaView, Alert, RefreshControl, ProgressViewIOS, Platform
} from 'react-native';

const API_BASE = 'http://192.168.1.100:8000';

// ---- Laundry Status Screen ----
function LaundryStatusScreen() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);

  const fetchLatest = async () => {
    try {
      const resp = await fetch(`${API_BASE}/api/status`);
      const json = await resp.json();
      setData(json);
    } catch (e) {
      console.error('Fetch failed:', e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchLatest();
    const iv = setInterval(fetchLatest, 5000);
    return () => clearInterval(iv);
  }, []);

  const fireRisk = data?.fire_risk || 0;
  const fireRiskPct = Math.round(fireRisk * 100);
  const fireLevel = fireRisk > 0.95 ? 'EMERGENCY' : fireRisk > 0.8 ? 'CRITICAL'
    : fireRisk > 0.6 ? 'WARNING' : 'OK';
  const fireColor = fireRisk > 0.8 ? '#f44336' : fireRisk > 0.6 ? '#ff9800' : '#4caf50';

  const phaseNames = ['Idle', 'Filling', 'Washing', 'Rinsing', 'Spinning', 'Done'];
  const dryerStates = ['Off', 'Heating', 'Tumbling', 'Cooling', 'Done'];
  const fabricNames = ['Unknown', 'Cotton', 'Polyester', 'Wool', 'Silk', 'Denim', 'Nylon', 'Linen', 'Blend'];

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>WashWise</Text>

      {/* Fire Risk Gauge — always visible */}
      <View style={[styles.fireGauge, {borderColor: fireColor}]}>
        <Text style={styles.gaugeLabel}>Fire Risk</Text>
        <Text style={[styles.gaugeValue, {color: fireColor}]}>{fireRiskPct}%</Text>
        <Text style={[styles.gaugeLevel, {color: fireColor}]}>{fireLevel}</Text>
      </View>

      <ScrollView refreshControl={<RefreshControl refreshing={loading} onRefresh={fetchLatest} />}>
        {/* Washer status */}
        {data?.washer && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>🫧 Washer</Text>
            <View style={styles.row}>
              <Text style={styles.label}>Phase</Text>
              <Text style={styles.value}>{phaseNames[data.washer.cycle_phase] || 'Unknown'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Water temp</Text>
              <Text style={styles.value}>{(data.washer.water_temp || 0).toFixed(1)}°C</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Detergent</Text>
              <Text style={styles.value}>{((data.washer.reservoir_g || 0)).toFixed(0)}g left</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Fabric detected</Text>
              <Text style={styles.value}>{fabricNames[data.washer.fabric_type] || 'Unknown'}</Text>
            </View>
            {data.washer.imbalance_flag > 0 && (
              <Text style={styles.warning}>⚠️ Load imbalance — rebalance!</Text>
            )}
            {data.washer.leak_flag > 0 && (
              <Text style={styles.critical}>🚨 Possible leak detected!</Text>
            )}
          </View>
        )}

        {/* Dryer status */}
        {data?.dryer && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>🔥 Dryer</Text>
            <View style={styles.row}>
              <Text style={styles.label}>State</Text>
              <Text style={styles.value}>{dryerStates[data.dryer.dryer_state] || 'Off'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Exhaust temp</Text>
              <Text style={styles.value}>{(data.dryer.exhaust_temp || 0).toFixed(1)}°C</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Lint clog</Text>
              <Text style={[styles.value,
                data.dryer.lint_clog_level >= 3 ? styles.critical :
                data.dryer.lint_clog_level >= 2 ? styles.warning : null]}>
                {['Clean', 'Mild', 'Moderate', 'Severe'][data.dryer.lint_clog_level] || 'Clean'}
              </Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Dryness</Text>
              <Text style={styles.value}>
                {['Wet', 'Damp', 'Dry', 'Over-dry'][data.dryer.dryness_level] || 'Unknown'}
              </Text>
            </View>
          </View>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Fire Safety Screen ----
function FireSafetyScreen() {
  const [risk, setRisk] = useState(null);
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    const fetchRisk = () => fetch(`${API_BASE}/api/fire_risk`)
      .then(r => r.json()).then(setRisk).catch(console.error);
    const fetchAlerts = () => fetch(`${API_BASE}/api/alerts?limit=50`)
      .then(r => r.json()).then(setAlerts).catch(console.error);
    fetchRisk(); fetchAlerts();
    const iv = setInterval(() => { fetchRisk(); }, 10000);
    return () => clearInterval(iv);
  }, []);

  const fireAlerts = alerts.filter(a => a.source === 'dryer');

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>🔥 Fire Safety</Text>
      {risk && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Current Risk: {risk.level}</Text>
          <Text style={styles.bigValue}>{Math.round(risk.risk_score * 100)}%</Text>
          <Text style={styles.label}>
            Dryer: {risk.dryer_active ? 'Running' : 'Off'}
          </Text>
        </View>
      )}
      <ScrollView style={{marginTop: 16}}>
        <Text style={styles.cardTitle}>Recent Fire Alerts</Text>
        {fireAlerts.length === 0 && <Text style={styles.label}>No alerts 🎉</Text>}
        {fireAlerts.map((a, i) => (
          <View key={i} style={[styles.alertCard,
            a.level === 'EMERGENCY' && styles.emergencyCard]}>
            <Text style={styles.alertLevel}>{a.level}</Text>
            <Text style={styles.alertMsg}>{a.message}</Text>
            <Text style={styles.alertTime}>{a.timestamp}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Scan History Screen ----
function ScanHistoryScreen() {
  const [scans, setScans] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/api/scans/history?limit=30`)
      .then(r => r.json()).then(setScans).catch(console.error);
  }, []);

  const fabricNames = ['Unknown', 'Cotton', 'Polyester', 'Wool', 'Silk', 'Denim', 'Nylon', 'Linen', 'Blend'];
  const stainNames = ['Clean', 'Coffee', 'Wine', 'Blood', 'Grease', 'Grass', 'Ink', 'Food', 'Sweat', 'Rust', 'Unknown'];

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>📸 Scan History</Text>
      <ScrollView>
        {scans.map((s, i) => (
          <View key={i} style={styles.card}>
            <View style={styles.row}>
              <Text style={styles.label}>Fabric</Text>
              <Text style={styles.value}>
                {fabricNames[s.fabric_type] || 'Unknown'} ({Math.round((s.fabric_confidence || 0) / 2.55)}%)
              </Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Stain</Text>
              <Text style={styles.value}>
                {stainNames[s.stain_type] || 'Unknown'} ({Math.round((s.stain_confidence || 0) / 2.55)}%)
              </Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Recommended</Text>
              <Text style={styles.value}>
                {s.wash_temp?.toFixed(0)}°C, {s.detergent_ml}mL det.
              </Text>
            </View>
            <Text style={styles.alertTime}>{s.timestamp}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Energy Report Screen ----
function EnergyScreen() {
  const [energy, setEnergy] = useState(null);

  useEffect(() => {
    fetch(`${API_BASE}/api/energy?days=30`)
      .then(r => r.json()).then(setEnergy).catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>⚡ Energy & Water</Text>
      {energy ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Last 30 days</Text>
          <View style={styles.row}>
            <Text style={styles.label}>Cycles</Text>
            <Text style={styles.value}>{energy.cycles}</Text>
          </View>
          <View style={styles.row}>
            <Text style={styles.label}>Energy</Text>
            <Text style={styles.value}>{(energy.total_energy_kwh || 0).toFixed(1)} kWh</Text>
          </View>
          <View style={styles.row}>
            <Text style={styles.label}>Water</Text>
            <Text style={styles.value}>{(energy.total_water_l || 0).toFixed(0)} L</Text>
          </View>
          <View style={styles.row}>
            <Text style={styles.label}>Cost</Text>
            <Text style={styles.value}>${(energy.total_cost_dollars || 0).toFixed(2)}</Text>
          </View>
          <View style={styles.row}>
            <Text style={styles.label}>CO₂</Text>
            <Text style={styles.value}>{(energy.total_co2_kg || 0).toFixed(1)} kg</Text>
          </View>
        </View>
      ) : (
        <Text style={styles.label}>Loading...</Text>
      )}
    </SafeAreaView>
  );
}

// ---- Setup Wizard ----
function SetupScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Setup Wizard</Text>
      <ScrollView>
        <Text style={styles.wizardStep}>1. Power on hub node (USB-C)</Text>
        <Text style={styles.wizardStep}>2. Connect via BLE to configure WiFi</Text>
        <Text style={styles.wizardStep}>3. Install dryer node FIRST (fire safety!)</Text>
        <Text style={styles.wizardStep}>   - Attach pressure taps to exhaust</Text>
        <Text style={styles.wizardStep}>   - Tape thermocouple to exhaust pipe</Text>
        <Text style={styles.wizardStep}>   - Clamp current sensor on dryer cord</Text>
        <Text style={styles.wizardStep}>   - Connect USB-C power</Text>
        <Text style={styles.wizardStep}>4. Install washer node</Text>
        <Text style={styles.wizardStep}>   - Mount vibration sensor on cabinet</Text>
        <Text style={styles.wizardStep}>   - Clamp current sensor on washer cord</Text>
        <Text style={styles.wizardStep}>   - Connect flow sensor to fill hose</Text>
        <Text style={styles.wizardStep}>   - Place detergent on load cell</Text>
        <Text style={styles.wizardStep}>   - Connect pump to dispenser drawer</Text>
        <Text style={styles.wizardStep}>5. Charge stain scanner (USB-C)</Text>
        <Text style={styles.wizardStep}>6. Run calibration (scripts/calibrate)</Text>
        <Text style={styles.wizardStep}>7. Load detergent into reservoir</Text>
        <Text style={styles.wizardStep}>8. Scan your first garment!</Text>
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Navigation ----
const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator>
        <Tab.Screen name="Laundry" component={LaundryStatusScreen} />
        <Tab.Screen name="Fire Safety" component={FireSafetyScreen} />
        <Tab.Screen name="Scans" component={ScanHistoryScreen} />
        <Tab.Screen name="Energy" component={EnergyScreen} />
        <Tab.Screen name="Setup" component={SetupScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: {flex: 1, backgroundColor: '#0a1628', padding: 16},
  title: {fontSize: 24, fontWeight: 'bold', color: '#4fc3f7', marginBottom: 16},
  fireGauge: {
    backgroundColor: '#1a2a44', borderRadius: 16, padding: 20,
    alignItems: 'center', marginBottom: 16, borderWidth: 2,
  },
  gaugeLabel: {color: '#b0bec5', fontSize: 14, marginBottom: 4},
  gaugeValue: {fontSize: 48, fontWeight: 'bold'},
  gaugeLevel: {fontSize: 16, fontWeight: 'bold', marginTop: 4},
  card: {backgroundColor: '#1a2a44', borderRadius: 12, padding: 16, marginBottom: 12},
  cardTitle: {fontSize: 16, fontWeight: 'bold', color: '#81d4fa', marginBottom: 8},
  row: {flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 6,
    borderBottomWidth: 0.5, borderBottomColor: '#2a3a54'},
  label: {color: '#b0bec5', fontSize: 14, flex: 1},
  value: {fontSize: 16, fontWeight: 'bold', color: '#eceff1', flex: 1, textAlign: 'right'},
  bigValue: {fontSize: 56, fontWeight: 'bold', color: '#eceff1', textAlign: 'center', marginVertical: 8},
  warning: {color: '#ff9800', fontSize: 14, marginTop: 8, fontWeight: 'bold'},
  critical: {color: '#f44336', fontSize: 14, marginTop: 8, fontWeight: 'bold'},
  alertCard: {backgroundColor: '#1a2a44', borderRadius: 8, padding: 12, marginBottom: 8},
  emergencyCard: {borderLeftWidth: 4, borderLeftColor: '#f44336'},
  alertLevel: {fontWeight: 'bold', color: '#ff9800', fontSize: 12},
  alertMsg: {color: '#eceff1', fontSize: 14},
  alertTime: {color: '#607d8b', fontSize: 11, marginTop: 4},
  wizardStep: {color: '#b0bec5', fontSize: 15, paddingVertical: 10,
    borderBottomWidth: 0.5, borderBottomColor: '#2a3a54'},
});