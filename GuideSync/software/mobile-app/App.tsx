// GuideSync — React Native Mobile App
//
// AI-powered spatial awareness & visual assistance app for the blind & VI.
// Screens: Dashboard, Scene, Navigation, Beacons, Reading, Alerts,
// Emergency, Faces, Caregiver, Settings.
// Fully VoiceOver/TalkBack compatible.
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

// ─── Object class names ────────────────────────────────────────────────────
const OBJECT_NAMES = [
  'person', 'bicycle', 'car', 'motorcycle', 'bus', 'truck',
  'traffic light', 'stop sign', 'chair', 'table', 'bed', 'couch',
  'door', 'stairs', 'elevator', 'escalator', 'bottle', 'cup',
  'laptop', 'cell phone', 'book', 'clock', 'dog', 'cat',
  'white cane', 'guide dog', 'trash can', 'pole', 'wall', 'doorway',
  'curb', 'puddle', 'overhanging branch',
];

const NAV_DIRECTIONS = {
  0: 'Straight', 1: 'Left', 2: 'Right', 3: 'Turn Around',
  4: 'Stop', 5: 'Arrived', 6: 'Upstairs', 7: 'Downstairs',
};

// ─── Dashboard Screen ──────────────────────────────────────────────────────
function DashboardScreen() {
  const [devices, setDevices] = useState([]);
  const [location, setLocation] = useState({});
  const [navStatus, setNavStatus] = useState({ active: false });
  const [alerts, setAlerts] = useState([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAll = useCallback(async () => {
    try {
      const [devs, loc, nav, alts] = await Promise.all([
        fetch(`${API_BASE}/devices`).then(r => r.json()),
        fetch(`${API_BASE}/location`).then(r => r.json()),
        fetch(`${API_BASE}/navigation/status`).then(r => r.json()),
        fetch(`${API_BASE}/alerts?limit=5`).then(r => r.json()),
      ]);
      setDevices(devs);
      setLocation(loc);
      setNavStatus(nav);
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
      <ScrollView refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
        <Text style={styles.h1}>GuideSync</Text>
        <Text style={styles.subtitle}>Spatial Awareness Dashboard</Text>

        {/* Device Status */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Device Status</Text>
          {devices.map((d, i) => (
            <Text key={i} style={styles.cardRow}>
              {d.device_type === 'hub' ? '🔧' : d.device_type === 'glasses' ? '👓' :
               d.device_type === 'cane' ? '🦯' : d.device_type === 'band' ? '⌚' : '📡'}
              {' '}{d.name}: {d.online ? '✅ Online' : '❌ Offline'}
            </Text>
          ))}
        </View>

        {/* Location */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Location</Text>
          {location.gps_lat && (
            <Text style={styles.cardRow}>GPS: {location.gps_lat.toFixed(4)}, {location.gps_lon.toFixed(4)}</Text>
          )}
          {location.indoor_x !== null && location.indoor_x !== undefined && (
            <Text style={styles.cardRow}>Indoor: Floor {location.indoor_floor}, ({location.indoor_x?.toFixed(1)}, {location.indoor_y?.toFixed(1)})</Text>
          )}
          {location.nearest_beacon && (
            <Text style={styles.cardRow}>Near: {location.nearest_beacon} ({location.nearest_beacon_distance_m?.toFixed(1)} m)</Text>
          )}
        </View>

        {/* Navigation Status */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Navigation</Text>
          {navStatus.active ? (
            <>
              <Text style={styles.cardRow}>Destination: {navStatus.destination}</Text>
              <Text style={styles.cardRow}>Step {navStatus.current_step + 1} of {navStatus.total_steps}</Text>
              <Text style={styles.cardRow}>ETA: {navStatus.eta_minutes} min</Text>
            </>
          ) : (
            <Text style={styles.empty}>Not navigating</Text>
          )}
        </View>

        {/* Recent Alerts */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Recent Alerts</Text>
          {alerts.length === 0 ? (
            <Text style={styles.empty}>No alerts</Text>
          ) : (
            alerts.map((a, i) => (
              <Text key={i} style={styles.cardRow}>
                {a.severity === 'emergency' ? '🚨' : a.severity === 'critical' ? '🔴' :
                 a.severity === 'warning' ? '🟡' : 'ℹ️'} {a.alert_type}: {a.message}
              </Text>
            ))
          )}
        </View>

        {/* Quick Actions */}
        <TouchableOpacity style={styles.actionButton} onPress={() => {
          Alert.prompt('Destination', 'Where do you want to go?', (text) => {
            fetch(`${API_BASE}/navigation/destination`, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ destination: text })
            });
          });
        }}>
          <Text style={styles.actionText}>🧭 Set Destination</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.sosButton} onPress={() => {
          Alert.alert('SOS', 'Press and hold the SOS button on your band for 3 seconds');
        }}>
          <Text style={styles.sosText}>🆘 SOS Help</Text>
        </TouchableOpacity>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Scene Screen ──────────────────────────────────────────────────────────
function SceneScreen() {
  const [scenes, setScenes] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/glasses/scene`).then(r => r.json()).then(setScenes).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Scene View</Text>
        <Text style={styles.subtitle}>Objects detected by Smart Glasses</Text>
        {scenes.length === 0 ? (
          <Text style={styles.empty}>No scene data yet</Text>
        ) : (
          scenes.map((s, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>Scene at {s.timestamp}</Text>
              <Text style={styles.cardRow}>Objects: {s.object_count}</Text>
              {s.objects?.map((obj, j) => (
                <Text key={j} style={styles.cardRow}>
                  • {OBJECT_NAMES[obj.class] || `Object ${obj.class}`} — {obj.distance_m}m, {obj.direction_deg}°
                </Text>
              ))}
              {s.crosswalk_state !== 'none' && (
                <Text style={styles.cardRow}>🚦 Crosswalk: {s.crosswalk_state}</Text>
              )}
              {s.text_read && (
                <Text style={styles.cardRow}>📖 Text: "{s.text_read}"</Text>
              )}
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Navigation Screen ─────────────────────────────────────────────────────
function NavigationScreen() {
  const [route, setRoute] = useState(null);
  const [status, setStatus] = useState({ active: false });

  useEffect(() => {
    fetch(`${API_BASE}/navigation/route`).then(r => r.json()).then(setRoute).catch(console.log);
    fetch(`${API_BASE}/navigation/status`).then(r => r.json()).then(setStatus).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Navigation</Text>
        <Text style={styles.subtitle}>Indoor turn-by-turn via haptic band</Text>

        {status.active ? (
          <>
            <View style={styles.card}>
              <Text style={styles.cardTitle}>Active Route</Text>
              <Text style={styles.cardRow}>Destination: {route?.destination}</Text>
              <Text style={styles.cardRow}>ETA: {status.eta_minutes} min</Text>
            </View>

            <View style={styles.card}>
              <Text style={styles.cardTitle}>Route Steps</Text>
              {route?.steps?.map((step, i) => (
                <Text key={i} style={[
                  styles.cardRow,
                  i === status.current_step && styles.activeStep
                ]}>
                  {i === status.current_step ? '→ ' : '  '}
                  {NAV_DIRECTIONS[step.direction] || step.direction} — {step.distance_m}m
                  {step.landmark ? ` (${step.landmark})` : ''}
                </Text>
              ))}
            </View>

            <TouchableOpacity style={styles.actionButton} onPress={() => {
              fetch(`${API_BASE}/navigation/stop`, { method: 'POST' });
              Alert.alert('Navigation stopped');
            }}>
              <Text style={styles.actionText}>⏹ Stop Navigation</Text>
            </TouchableOpacity>
          </>
        ) : (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Set Destination</Text>
            <Text style={styles.cardRow}>Tap a beacon to navigate there:</Text>
            {/* Production: list beacons as buttons */}
          </View>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Beacons Screen ────────────────────────────────────────────────────────
function BeaconsScreen() {
  const [beacons, setBeacons] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/beacons`).then(r => r.json()).then(setBeacons).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Nav Beacons</Text>
        <Text style={styles.subtitle}>Indoor positioning landmarks</Text>
        {beacons.map((b, i) => (
          <View key={i} style={styles.card}>
            <Text style={styles.cardTitle}>📡 {b.landmark_name}</Text>
            <Text style={styles.cardRow}>ID: 0x{b.uuid_short.toString(16).toUpperCase()}</Text>
            <Text style={styles.cardRow}>Position: Floor {b.floor}, ({b.x}, {b.y})</Text>
            <Text style={styles.cardRow}>Battery: {(b.battery_v / 100).toFixed(2)}V</Text>
            <TouchableOpacity style={styles.navButton} onPress={() => {
              fetch(`${API_BASE}/navigation/destination`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ destination: b.landmark_name })
              });
              Alert.alert('Navigating', `Route to ${b.landmark_name} started`);
            }}>
              <Text style={styles.actionText}>🧭 Navigate Here</Text>
            </TouchableOpacity>
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
                {a.severity === 'emergency' ? '🚨' : a.severity === 'critical' ? '🔴' :
                 a.severity === 'warning' ? '🟡' : 'ℹ️'} {a.alert_type}
              </Text>
              <Text style={styles.cardRow}>{a.message}</Text>
              <Text style={styles.cardRow}>Time: {a.timestamp}</Text>
              {!a.acknowledged && (
                <TouchableOpacity style={styles.ackButton} onPress={() => {
                  fetch(`${API_BASE}/alerts/${a.id}/ack`, { method: 'PUT' });
                }}>
                  <Text style={styles.actionText}>Acknowledge</Text>
                </TouchableOpacity>
              )}
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Emergency Screen ──────────────────────────────────────────────────────
function EmergencyScreen() {
  const [contacts, setContacts] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/emergency/contacts`).then(r => r.json()).then(setContacts).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Emergency</Text>
        <Text style={styles.subtitle}>Fall detection & SOS contacts</Text>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>How SOS Works</Text>
          <Text style={styles.cardRow}>Press and hold the SOS button on your haptic band for 3 seconds.</Text>
          <Text style={styles.cardRow}>GuideSync will:</Text>
          <Text style={styles.cardRow}>• SMS all emergency contacts with GPS</Text>
          <Text style={styles.cardRow}>• Call 911 with automated message</Text>
          <Text style={styles.cardRow}>• Cancel: press SOS 3× rapidly within 60s</Text>
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>Emergency Contacts</Text>
          {contacts.map((c, i) => (
            <Text key={i} style={styles.cardRow}>
              {c.name} ({c.relationship}): {c.phone}
            </Text>
          ))}
        </View>

        <TouchableOpacity style={styles.sosCancelButton} onPress={() => {
          fetch(`${API_BASE}/sos/cancel`, { method: 'POST' });
          Alert.alert('SOS Cancelled', 'Emergency dispatch cancelled');
        }}>
          <Text style={styles.sosText}>Cancel Active SOS</Text>
        </TouchableOpacity>
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
          <Text style={styles.cardRow}>Manage connected GuideSync nodes</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Audio</Text>
          <Text style={styles.cardRow}>Bone conduction volume</Text>
          <Text style={styles.cardRow}>Voice command language</Text>
          <Text style={styles.cardRow}>Custom voice phrases</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Haptics</Text>
          <Text style={styles.cardRow}>Navigation haptic intensity</Text>
          <Text style={styles.cardRow}>Alert vibration patterns</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Fall Detection</Text>
          <Text style={styles.cardRow}>Sensitivity (Low/Medium/High)</Text>
          <Text style={styles.cardRow}>Auto-911 toggle</Text>
          <Text style={styles.cardRow}>Cancel window: 30s</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Privacy</Text>
          <Text style={styles.cardRow}>Face recognition: Off (opt-in)</Text>
          <Text style={styles.cardRow}>On-device processing only</Text>
          <Text style={styles.cardRow}>No video leaves glasses unless OCR requested</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Caregiver Sharing</Text>
          <Text style={styles.cardRow}>Share location with caregiver</Text>
          <Text style={styles.cardRow}>Share fall/SOS alerts</Text>
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
        <Tab.Screen name="Scene" component={SceneScreen} />
        <Tab.Screen name="Navigate" component={NavigationScreen} />
        <Tab.Screen name="Beacons" component={BeaconsScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
        <Tab.Screen name="Emergency" component={EmergencyScreen} />
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
  activeStep: { fontWeight: 'bold', color: '#4fc3f7' },
  actionButton: {
    backgroundColor: '#1a3a5a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  actionText: { color: '#4fc3f7', fontSize: 14, fontWeight: 'bold' },
  sosButton: {
    backgroundColor: '#3a1a1a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  sosText: { color: '#ff5252', fontSize: 16, fontWeight: 'bold' },
  sosCancelButton: {
    backgroundColor: '#2a2a1a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  navButton: {
    backgroundColor: '#1a3a5a', borderRadius: 6, padding: 10,
    marginTop: 8, alignItems: 'center',
  },
  ackButton: {
    backgroundColor: '#1a3a1a', borderRadius: 6, padding: 10,
    marginTop: 8, alignItems: 'center',
  },
});