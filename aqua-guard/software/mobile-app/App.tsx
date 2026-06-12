/**
 * App.tsx — Aqua Guard Mobile App (React Native)
 * 
 * Navigation: Home → TankOverview, AlertHistory, FeedLog, CameraView, SetupWizard
 */

import React, {useState, useEffect} from 'react';
import {NavigationContainer} from '@react-navigation/native';
import {createBottomTabNavigator} from '@react-navigation/bottom-tabs';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity,
  SafeAreaView, Alert, RefreshControl
} from 'react-native';

// ---- Tank Overview Screen ----
function TankOverviewScreen() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);

  const fetchLatest = async () => {
    try {
      const resp = await fetch('http://192.168.1.100:8000/api/sensors/latest');
      const json = await resp.json();
      setData(json);
    } catch (e) {
      console.error('Fetch failed:', e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchLatest(); const iv = setInterval(fetchLatest, 5000); return () => clearInterval(iv); }, []);

  const paramColors = {
    ph: '#2196F3', temperature: '#FF9800', ammonia: '#F44336',
    nitrite: '#E91E63', nitrate: '#9C27B0', dissolved_o2: '#4CAF50',
    tds: '#00BCD4', turbidity: '#795548'
  };

  const paramLabels = {
    ph: 'pH', temperature: 'Temp °C', ammonia: 'NH3 ppm',
    nitrite: 'NO2 ppm', nitrate: 'NO3 ppm', dissolved_o2: 'DO mg/L',
    tds: 'TDS µS/cm', turbidity: 'NTU'
  };

  const paramRanges = {
    ph: [6.5, 7.5], temperature: [24, 28], ammonia: [0, 0.25],
    nitrite: [0, 0.25], nitrate: [0, 40], dissolved_o2: [5, 12],
    tds: [150, 300], turbidity: [0, 50]
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Aqua Guard</Text>
      <ScrollView refreshControl={<RefreshControl refreshing={loading} onRefresh={fetchLatest} />}>
        {data && Object.entries(data).map(([key, node]) => (
          <View key={key} style={styles.nodeCard}>
            <Text style={styles.nodeTitle}>Sensor {key.replace('sensor_', '')}</Text>
            {Object.entries(paramLabels).map(([param, label]) => {
              const val = node[param];
              if (val === null || val === undefined) return null;
              const [lo, hi] = paramRanges[param];
              const inRange = val >= lo && val <= hi;
              return (
                <View key={param} style={[styles.paramRow, !inRange && styles.paramWarning]}>
                  <Text style={styles.paramLabel}>{label}</Text>
                  <Text style={[styles.paramValue, {color: paramColors[param]}]}>
                    {typeof val === 'number' ? val.toFixed(2) : val}
                  </Text>
                  <Text style={styles.paramRange}>
                    {inRange ? 'OK' : `Range: ${lo}-${hi}`}
                  </Text>
                </View>
              );
            })}
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Alert History Screen ----
function AlertHistoryScreen() {
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    fetch('http://192.168.1.100:8000/api/alerts?limit=50')
      .then(r => r.json())
      .then(setAlerts)
      .catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Alerts</Text>
      <ScrollView>
        {alerts.map((a, i) => (
          <View key={i} style={[styles.alertCard, a.level === 'CRITICAL' && styles.criticalCard]}>
            <Text style={styles.alertLevel}>{a.level}</Text>
            <Text style={styles.alertMsg}>{a.message}</Text>
            <Text style={styles.alertTime}>{a.timestamp}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Feed + Dose Screen ----
function FeedLogScreen() {
  const [log, setLog] = useState([]);
  const [feeding, setFeeding] = useState(false);

  const feed = async (portions: number) => {
    setFeeding(true);
    try {
      await fetch('http://192.168.1.100:8000/api/feed', {
        method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({portions})
      });
      Alert.alert('Fed!', `${portions} portion(s) dispensed`);
    } catch { Alert.alert('Error', 'Failed to feed'); }
    finally { setFeeding(false); }
  };

  useEffect(() => {
    fetch('http://192.168.1.100:8000/api/dosing_log?limit=50')
      .then(r => r.json())
      .then(setLog)
      .catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Feeding & Dosing</Text>
      <View style={styles.feedButtons}>
        <TouchableOpacity style={styles.feedBtn} onPress={() => feed(1)} disabled={feeding}>
          <Text style={styles.feedBtnText}>Feed 1 Portion</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.feedBtn} onPress={() => feed(2)} disabled={feeding}>
          <Text style={styles.feedBtnText}>Feed 2 Portions</Text>
        </TouchableOpacity>
      </View>
      <ScrollView>
        {log.map((entry, i) => (
          <View key={i} style={styles.logRow}>
            <Text>Pump {entry.pump_id}: {entry.volume_ml}mL — {entry.reason}</Text>
            <Text style={styles.logTime}>{entry.timestamp}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Camera Screen (placeholder) ----
function CameraViewScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Tank Camera</Text>
      <View style={styles.cameraPlaceholder}>
        <Text style={styles.cameraText}>Live camera feed from feeder node</Text>
        <Text style={styles.cameraSub}>Requires hub connection + feeder node camera</Text>
      </View>
    </SafeAreaView>
  );
}

// ---- Setup Wizard (placeholder) ----
function SetupWizardScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Setup Wizard</Text>
      <ScrollView>
        <Text style={styles.wizardStep}>1. Power on hub node</Text>
        <Text style={styles.wizardStep}>2. Connect via BLE</Text>
        <Text style={styles.wizardStep}>3. Configure WiFi</Text>
        <Text style={styles.wizardStep}>4. Add sensor node(s)</Text>
        <Text style={styles.wizardStep}>5. Calibrate sensors</Text>
        <Text style={styles.wizardStep}>6. Add feeder node</Text>
        <Text style={styles.wizardStep}>7. Load chemicals into pumps</Text>
        <Text style={styles.wizardStep}>8. Set tank type (tropical/reef/coldwater)</Text>
        <Text style={styles.wizardStep}>9. Enjoy your self-regulating aquarium!</Text>
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
        <Tab.Screen name="Tank" component={TankOverviewScreen} />
        <Tab.Screen name="Alerts" component={AlertHistoryScreen} />
        <Tab.Screen name="Feed" component={FeedLogScreen} />
        <Tab.Screen name="Camera" component={CameraViewScreen} />
        <Tab.Screen name="Setup" component={SetupWizardScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a1628', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#4fc3f7', marginBottom: 16 },
  nodeCard: { backgroundColor: '#1a2a44', borderRadius: 12, padding: 16, marginBottom: 12 },
  nodeTitle: { fontSize: 16, fontWeight: 'bold', color: '#81d4fa', marginBottom: 8 },
  paramRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 4, borderBottomWidth: 0.5, borderBottomColor: '#2a3a54' },
  paramWarning: { backgroundColor: 'rgba(255, 152, 0, 0.15)', borderRadius: 4 },
  paramLabel: { color: '#b0bec5', fontSize: 14, flex: 1 },
  paramValue: { fontSize: 16, fontWeight: 'bold', flex: 1, textAlign: 'right' },
  paramRange: { color: '#4caf50', fontSize: 12, flex: 1, textAlign: 'right' },
  alertCard: { backgroundColor: '#1a2a44', borderRadius: 8, padding: 12, marginBottom: 8 },
  criticalCard: { borderLeftWidth: 4, borderLeftColor: '#f44336' },
  alertLevel: { fontWeight: 'bold', color: '#ff9800', fontSize: 12 },
  alertMsg: { color: '#eceff1', fontSize: 14 },
  alertTime: { color: '#607d8b', fontSize: 11, marginTop: 4 },
  feedButtons: { flexDirection: 'row', gap: 12, marginBottom: 16 },
  feedBtn: { backgroundColor: '#1565c0', borderRadius: 8, padding: 16, flex: 1 },
  feedBtnText: { color: 'white', textAlign: 'center', fontWeight: 'bold' },
  logRow: { paddingVertical: 8, borderBottomWidth: 0.5, borderBottomColor: '#2a3a54' },
  logTime: { color: '#607d8b', fontSize: 11 },
  cameraPlaceholder: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  cameraText: { color: '#81d4fa', fontSize: 18, textAlign: 'center' },
  cameraSub: { color: '#607d8b', fontSize: 14, marginTop: 8 },
  wizardStep: { color: '#b0bec5', fontSize: 16, paddingVertical: 12, borderBottomWidth: 0.5, borderBottomColor: '#2a3a54' },
});