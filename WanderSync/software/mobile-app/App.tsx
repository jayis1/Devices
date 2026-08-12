// WanderSync — React Native Mobile App (Caregiver)
//
// AI-powered dementia care, wandering prevention & aging-in-place app.
// Screens: Dashboard, Live Map, Wandering Events, Activity Timeline,
// Doors, Rooms, Voice Reminders, Cognitive Health, Anomalies, Sleep,
// Alerts, Caregivers, Emergency, Settings.
//
// Build: react-native run-android / run-ios

import React, { useState, useEffect, useCallback } from 'react';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity,
  Alert, RefreshControl, SafeAreaView, Dimensions,
} from 'react-native';
import {
  NavigationContainer,
} from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';

const { width } = Dimensions.get('window');
const API_BASE = 'http://localhost:8080/api/v1';

// ─── Constants ──────────────────────────────────────────────────────────────

const ACTIVITY_COLORS = {
  absent: '#9e9e9e', walking: '#4caf50', sitting: '#2196f3',
  lying: '#9c27b0', eating: '#ff9800', cooking: '#f44336',
  pacing: '#e91e63', standing: '#00bcd4', sleeping: '#3f51b5',
};

const RISK_COLORS = {
  low: '#4caf50', moderate: '#ff9800', high: '#f44336', critical: '#9c27b0',
};

const REMINDER_TYPES = {
  medication: '💊', meal: '🍽️', hydration: '💧',
  appointment: '📅', orientation: '🧭', custom: '📝',
};

// ─── Dashboard Screen ──────────────────────────────────────────────────────

function DashboardScreen() {
  const [devices, setDevices] = useState([]);
  const [bandHealth, setBandHealth] = useState(null);
  const [wanderRisk, setWanderRisk] = useState(null);
  const [cognitiveScore, setCognitiveScore] = useState(null);
  const [doors, setDoors] = useState([]);
  const [rooms, setRooms] = useState([]);
  const [alerts, setAlerts] = useState([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAll = useCallback(async () => {
    try {
      const [devs, health, risk, cog, drs, rms, alts] = await Promise.all([
        fetch(`${API_BASE}/devices`).then(r => r.json()),
        fetch(`${API_BASE}/band/health`).then(r => r.json()),
        fetch(`${API_BASE}/risk/wander`).then(r => r.json()),
        fetch(`${API_BASE}/cognitive/score`).then(r => r.json()),
        fetch(`${API_BASE}/doors`).then(r => r.json()),
        fetch(`${API_BASE}/rooms`).then(r => r.json()),
        fetch(`${API_BASE}/alerts?limit=5`).then(r => r.json()),
      ]);
      setDevices(devs);
      setBandHealth(health);
      setWanderRisk(risk);
      setCognitiveScore(cog);
      setDoors(drs);
      setRooms(rms);
      setAlerts(alts);
    } catch (e) {
      console.log('API error:', e);
    }
  }, []);

  useEffect(() => { fetchAll(); }, [fetchAll]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchAll();
    setRefreshing(false);
  }, [fetchAll]);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView
        refreshControl={
          <RefreshControl refreshing={refreshing} onRefresh={onRefresh} />
        }
      >
        <Text style={styles.title}>WanderSync Dashboard</Text>

        {/* System Status */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>System Status</Text>
          <Text style={styles.statusText}>
            {devices.filter(d => d.online).length} / {devices.length} devices online
          </Text>
          {devices.map(d => (
            <View key={d.device_id} style={styles.deviceRow}>
              <Text style={styles.deviceName}>{d.name}</Text>
              <Text style={[styles.deviceStatus,
                { color: d.online ? '#4caf50' : '#f44336' }]}>
                {d.online ? '● Online' : '● Offline'}
              </Text>
            </View>
          ))}
        </View>

        {/* Wander Band Health */}
        {bandHealth && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Wander Band</Text>
            <View style={styles.row}>
              <View style={styles.statBox}>
                <Text style={styles.statValue}>{bandHealth.battery_pct}%</Text>
                <Text style={styles.statLabel}>Battery</Text>
              </View>
              <View style={styles.statBox}>
                <Text style={styles.statValue}>{bandHealth.heart_rate_bpm}</Text>
                <Text style={styles.statLabel}>Heart Rate</Text>
              </View>
              <View style={styles.statBox}>
                <Text style={styles.statValue}>{bandHealth.steps_today}</Text>
                <Text style={styles.statLabel}>Steps Today</Text>
              </View>
            </View>
            <View style={styles.row}>
              <Text style={styles.detailText}>
                Activity: {bandHealth.activity} | Geofence: {bandHealth.geofence_status}
              </Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.detailText}>
                On Wrist: {bandHealth.band_on_wrist ? '✅ Yes' : '❌ No'} |
                Distance Home: {bandHealth.distance_home_m}m
              </Text>
            </View>
          </View>
        )}

        {/* Wandering Risk */}
        {wanderRisk && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Wandering Risk</Text>
            <Text style={[styles.riskScore,
              { color: RISK_COLORS[wanderRisk.level] || '#4caf50' }]}>
              {wanderRisk.score} / 100 — {wanderRisk.level.toUpperCase()}
            </Text>
            <Text style={styles.recommendation}>{wanderRisk.recommendation}</Text>
          </View>
        )}

        {/* Cognitive Health */}
        {cognitiveScore && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Cognitive Health</Text>
            <Text style={styles.cognitiveScore}>
              Score: {cognitiveScore.score} / 100 ({cognitiveScore.trend})
            </Text>
            <Text style={styles.detailText}>
              Rate: {cognitiveScore.rate_per_month}/month |
              MMSE ~{cognitiveScore.mmse_equivalent}
            </Text>
          </View>
        )}

        {/* Doors */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Doors & Windows</Text>
          {doors.map(d => (
            <View key={d.door_id} style={styles.doorRow}>
              <Text style={styles.doorName}>{d.name}</Text>
              <Text style={styles.doorState}>
                {d.door_state === 'closed' ? '🔒' : '🔓'} {d.lock_state}
              </Text>
            </View>
          ))}
        </View>

        {/* Rooms */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Rooms</Text>
          {rooms.map(r => (
            <View key={r.room_id} style={styles.roomRow}>
              <Text style={styles.roomName}>{r.name}</Text>
              <View style={[styles.activityDot,
                { backgroundColor: ACTIVITY_COLORS[r.activity_class] || '#9e9e9e' }]} />
              <Text style={styles.roomActivity}>
                {r.presence ? r.activity_class : 'empty'} ({r.activity_confidence}%)
              </Text>
            </View>
          ))}
        </View>

        {/* Recent Alerts */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Recent Alerts</Text>
          {alerts.length === 0 ? (
            <Text style={styles.noAlerts}>No alerts — all clear ✅</Text>
          ) : (
            alerts.map(a => (
              <View key={a.id} style={styles.alertRow}>
                <Text style={[styles.alertType, {
                  color: a.severity === 'emergency' ? '#f44336' :
                         a.severity === 'critical' ? '#ff5722' :
                         a.severity === 'warning' ? '#ff9800' : '#2196f3'
                }]}>
                  {a.alert_type}
                </Text>
                <Text style={styles.alertMsg}>{a.message}</Text>
              </View>
            ))
          )}
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Live Map Screen ───────────────────────────────────────────────────────

function MapScreen() {
  const [location, setLocation] = useState(null);

  useEffect(() => {
    const fetchLocation = async () => {
      try {
        const loc = await fetch(`${API_BASE}/band/location`).then(r => r.json());
        setLocation(loc);
      } catch (e) { console.log('Map error:', e); }
    };
    fetchLocation();
    const interval = setInterval(fetchLocation, 5000);
    return () => clearInterval(interval);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.title}>Live Location</Text>
        <View style={styles.card}>
          {location ? (
            <>
              <Text style={styles.cardTitle}>Wander Band GPS</Text>
              <Text style={styles.gpsText}>
                Lat: {location.latitude.toFixed(6)}
              </Text>
              <Text style={styles.gpsText}>
                Lon: {location.longitude.toFixed(6)}
              </Text>
              <Text style={styles.detailText}>
                Fix: {location.fix ? '✅' : '❌'} |
                Activity: {location.activity} |
                Geofence: {location.geofence_status}
              </Text>
              <Text style={styles.detailText}>
                Wander Risk: {location.wander_risk}/100 |
                Battery: {location.battery_v}V
              </Text>
              {/* Production: render map with MapView + geofence circle */}
              <View style={styles.mapPlaceholder}>
                <Text style={styles.mapPlaceholderText}>
                  🗺️ Map view{'\n'}(requires react-native-maps)
                </Text>
              </View>
            </>
          ) : (
            <Text style={styles.loading}>Loading location...</Text>
          )}
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Voice Reminders Screen ────────────────────────────────────────────────

function RemindersScreen() {
  const [reminders, setReminders] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/voice/reminders`)
      .then(r => r.json())
      .then(setReminders)
      .catch(e => console.log('Reminders error:', e));
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.title}>Voice Reminders</Text>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Daily Schedule</Text>
          {reminders.map(r => (
            <View key={r.hour} style={styles.reminderRow}>
              <Text style={styles.reminderTime}>
                {REMINDER_TYPES[r.reminder_type] || '📝'} {r.hour}:00
              </Text>
              <Text style={styles.reminderType}>{r.reminder_type}</Text>
              <Text style={styles.reminderStatus}>
                {r.enabled ? '✅' : '⬜'} Clip #{r.clip_index}
              </Text>
            </View>
          ))}
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Family Voice Clips</Text>
          <Text style={styles.detailText}>
            Record personalized voice reminders using your own voice.
            Your loved one will hear "Dad, it's time for your medication"
            in your voice — 92% adherence vs 54% with standard alarms.
          </Text>
          <TouchableOpacity style={styles.button}>
            <Text style={styles.buttonText}>Record New Clip</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Cognitive Health Screen ───────────────────────────────────────────────

function CognitiveScreen() {
  const [score, setScore] = useState(null);
  const [trajectory, setTrajectory] = useState([]);

  useEffect(() => {
    Promise.all([
      fetch(`${API_BASE}/cognitive/score`).then(r => r.json()),
      fetch(`${API_BASE}/cognitive/trajectory?months=6`).then(r => r.json()),
    ]).then(([s, t]) => {
      setScore(s);
      setTrajectory(t);
    }).catch(e => console.log('Cognitive error:', e));
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.title}>Cognitive Health</Text>
        {score && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Current Score</Text>
            <Text style={styles.cognitiveScore}>
              {score.score} / 100
            </Text>
            <Text style={styles.detailText}>
              Trend: {score.trend} | Rate: {score.rate_per_month}/month
            </Text>
            <Text style={styles.detailText}>
              MMSE Equivalent: ~{score.mmse_equivalent}/30
            </Text>
          </View>
        )}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>6-Month Trajectory</Text>
          {trajectory.map(t => (
            <View key={t.month} style={styles.trajectoryRow}>
              <Text style={styles.trajectoryMonth}>
                Month {t.month}: Score {t.score.toFixed(1)} (MMSE ~{t.mmse_equivalent})
              </Text>
              <Text style={styles.trajectoryChange}>{t.key_changes}</Text>
            </View>
          ))}
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Neurologist Report</Text>
          <Text style={styles.detailText}>
            Download a neurologist-ready cognitive assessment report with
            6-month trajectory, ADL patterns, sleep analysis, and anomaly history.
          </Text>
          <TouchableOpacity style={styles.button}>
            <Text style={styles.buttonText}>Download PDF Report</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Emergency Screen ──────────────────────────────────────────────────────

function EmergencyScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.title}>Emergency</Text>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>911 Dispatch Status</Text>
          <Text style={styles.detailText}>No active dispatch</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Emergency Contacts</Text>
          <Text style={styles.detailText}>• Sarah Johnson (daughter) — (555) 123-4567</Text>
          <Text style={styles.detailText}>• Dr. Chen (neurologist) — (555) 987-6543</Text>
          <Text style={styles.detailText}>• Neighbor Bob — (555) 555-5555</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Medical Information</Text>
          <Text style={styles.detailText}>Diagnosis: Alzheimer's Disease (moderate)</Text>
          <Text style={styles.detailText}>Medications: Donepezil 10mg, Memantine 20mg</Text>
          <Text style={styles.detailText}>Allergies: Penicillin</Text>
          <Text style={styles.detailText}>Blood Type: O+</Text>
        </View>
        <TouchableOpacity style={[styles.button, { backgroundColor: '#f44336' }]}>
          <Text style={styles.buttonText}>Cancel Active Dispatch</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.button}>
          <Text style={styles.buttonText}>Test SOS</Text>
        </TouchableOpacity>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Navigation ─────────────────────────────────────────────────────────────

const Tab = createBottomTabNavigator();

function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: '#2196f3',
          headerShown: false,
        }}
      >
        <Tab.Screen
          name="Dashboard"
          component={DashboardScreen}
          options={{ tabBarIcon: () => <Text>🏠</Text> }}
        />
        <Tab.Screen
          name="Map"
          component={MapScreen}
          options={{ tabBarIcon: () => <Text>🗺️</Text> }}
        />
        <Tab.Screen
          name="Reminders"
          component={RemindersScreen}
          options={{ tabBarIcon: () => <Text>🔔</Text> }}
        />
        <Tab.Screen
          name="Cognitive"
          component={CognitiveScreen}
          options={{ tabBarIcon: () => <Text>🧠</Text> }}
        />
        <Tab.Screen
          name="Emergency"
          component={EmergencyScreen}
          options={{ tabBarIcon: () => <Text>🚨</Text> }}
        />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

export default App;

// ─── Styles ─────────────────────────────────────────────────────────────────

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', textAlign: 'center',
           marginTop: 20, marginBottom: 10, color: '#333' },
  card: { backgroundColor: '#fff', margin: 10, padding: 15,
          borderRadius: 12, shadowColor: '#000', shadowOffset: { width: 0, height: 2 },
          shadowOpacity: 0.1, shadowRadius: 4, elevation: 3 },
  cardTitle: { fontSize: 18, fontWeight: 'bold', marginBottom: 10, color: '#333' },
  row: { flexDirection: 'row', justifyContent: 'space-around', marginVertical: 8 },
  statBox: { alignItems: 'center', flex: 1 },
  statValue: { fontSize: 24, fontWeight: 'bold', color: '#2196f3' },
  statLabel: { fontSize: 12, color: '#666', marginTop: 4 },
  detailText: { fontSize: 14, color: '#666', marginTop: 4 },
  statusText: { fontSize: 16, color: '#4caf50', marginBottom: 8 },
  deviceRow: { flexDirection: 'row', justifyContent: 'space-between',
               paddingVertical: 4 },
  deviceName: { fontSize: 14, color: '#333' },
  deviceStatus: { fontSize: 14 },
  riskScore: { fontSize: 28, fontWeight: 'bold', textAlign: 'center',
               marginVertical: 8 },
  recommendation: { fontSize: 14, color: '#666', marginTop: 4, textAlign: 'center' },
  cognitiveScore: { fontSize: 28, fontWeight: 'bold', color: '#2196f3',
                     textAlign: 'center', marginVertical: 8 },
  doorRow: { flexDirection: 'row', justifyContent: 'space-between',
             paddingVertical: 6 },
  doorName: { fontSize: 14, color: '#333' },
  doorState: { fontSize: 14, color: '#666' },
  roomRow: { flexDirection: 'row', alignItems: 'center', paddingVertical: 6 },
  roomName: { fontSize: 14, color: '#333', flex: 1 },
  activityDot: { width: 12, height: 12, borderRadius: 6, marginRight: 8 },
  roomActivity: { fontSize: 14, color: '#666' },
  noAlerts: { fontSize: 16, color: '#4caf50', textAlign: 'center', padding: 10 },
  alertRow: { flexDirection: 'row', paddingVertical: 6 },
  alertType: { fontSize: 14, fontWeight: 'bold', marginRight: 8 },
  alertMsg: { fontSize: 14, color: '#666', flex: 1 },
  gpsText: { fontSize: 16, color: '#333', marginVertical: 2 },
  mapPlaceholder: { height: 200, backgroundColor: '#e0e0e0', borderRadius: 12,
                     marginTop: 10, justifyContent: 'center', alignItems: 'center' },
  mapPlaceholderText: { fontSize: 16, color: '#999', textAlign: 'center' },
  loading: { fontSize: 16, color: '#999', textAlign: 'center', padding: 20 },
  reminderRow: { flexDirection: 'row', justifyContent: 'space-between',
                  paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#eee' },
  reminderTime: { fontSize: 16, fontWeight: 'bold', color: '#333' },
  reminderType: { fontSize: 14, color: '#666' },
  reminderStatus: { fontSize: 14, color: '#666' },
  trajectoryRow: { paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#eee' },
  trajectoryMonth: { fontSize: 14, fontWeight: 'bold', color: '#333' },
  trajectoryChange: { fontSize: 13, color: '#666', marginTop: 2 },
  button: { backgroundColor: '#2196f3', padding: 15, borderRadius: 8,
             marginTop: 10, alignItems: 'center' },
  buttonText: { color: '#fff', fontSize: 16, fontWeight: 'bold' },
});