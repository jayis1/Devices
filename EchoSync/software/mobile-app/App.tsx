/**
 * EchoSync — Sound Awareness System Mobile App
 * React Native + Expo
 *
 * Real-time sound event feed, haptic alerts, custom sound enrollment,
 * daily sound logs, caregiver sharing, and accessibility settings.
 *
 * 8 Screens:
 * 1. Dashboard — Real-time sound event feed + awareness score
 * 2. Live Events — Scrolling sound event list with direction
 * 3. Room Map — Home floor plan with sentinel coverage
 * 4. Wrist Band — Battery, worn/sleep status, haptic config
 * 5. Custom Sounds — Enroll custom doorbell/phone/alarm sounds
 * 6. History — Daily/weekly/monthly sound event charts
 * 7. Alerts — Push notification history + caregiver sharing
 * 8. Settings — Device management, haptic intensity, accessibility profile
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  StyleSheet, Text, View, ScrollView, FlatList, TouchableOpacity,
  SafeAreaView, StatusBar, Animated, Vibration, Platform, Alert as RNAlert,
} from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';

// === Types ===

interface SoundEvent {
  node_id: number;
  sound_class: number;
  sound_name: string;
  confidence: number;
  direction: number;
  priority: number;
  event_id: number;
  timestamp: string;
  room?: string;
}

interface WristBandStatus {
  node_id: number;
  battery_v: number;
  worn: boolean;
  sleeping: boolean;
  alerts_24h: number;
}

// === Constants ===

const SOUND_ICONS: { [key: string]: string } = {
  SmokeAlarm: '🔥', COAlarm: '⚠️', GlassBreak: '💥', Siren: '🚨',
  Doorbell: '🔔', DoorKnock: '✊', PhoneRing: '📞', BabyCry: '👶',
  CarHorn: '🚗', DoorOpen: '🚪', DoorClose: '🚪', Water: '💧',
  DogBark: '🐕', AlarmClock: '⏰', Microwave: '🍲', Dishwasher: '🍽️',
  WashingMachine: '🌀', PersonEnter: '🚶', Custom1: '★', Custom2: '★',
};

const PRIORITY_COLORS = {
  0: '#2196F3',  // Info - Blue
  1: '#FF9800',  // Important - Orange
  2: '#F44336',  // Emergency - Red
};

const PRIORITY_LABELS = {
  0: 'Info',
  1: 'Important',
  2: 'Emergency',
};

const API_BASE = 'http://localhost:8000/api/v1';

// === Dashboard Screen ===

function DashboardScreen() {
  const [events, setEvents] = useState<SoundEvent[]>([]);
  const [awarenessScore, setAwarenessScore] = useState(0);
  const [todayCount, setTodayCount] = useState(0);

  useEffect(() => {
    fetchEvents();
    const interval = setInterval(fetchEvents, 5000);
    return () => clearInterval(interval);
  }, []);

  const fetchEvents = async () => {
    try {
      const res = await fetch(`${API_BASE}/sound-events?limit=20`);
      const data = await res.json();
      setEvents(data);
    } catch (e) {
      console.log('Fetch error:', e);
    }
    try {
      const res = await fetch(`${API_BASE}/sound-awareness-score`);
      const data = await res.json();
      setAwarenessScore(data.score);
      setTodayCount(data.events_today);
    } catch (e) { /* offline */ }
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <View style={styles.header}>
          <Text style={styles.title}>EchoSync</Text>
          <Text style={styles.subtitle}>Sound Awareness System</Text>
        </View>

        {/* Awareness Score */}
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Sound Awareness Score</Text>
          <Text style={styles.scoreText}>{awarenessScore}%</Text>
          <Text style={styles.subText}>
            {todayCount} events today • {events.length} recent
          </Text>
        </View>

        {/* Latest Alert (highlighted) */}
        {events.length > 0 && (
          <View style={[styles.card, {
            backgroundColor: PRIORITY_COLORS[events[0].priority as keyof typeof PRIORITY_COLORS] + '22',
            borderColor: PRIORITY_COLORS[events[0].priority as keyof typeof PRIORITY_COLORS],
            borderWidth: 2,
          }]}>
            <View style={styles.alertHeader}>
              <Text style={styles.alertIcon}>
                {SOUND_ICONS[events[0].sound_name] || '🔊'}
              </Text>
              <View style={styles.alertInfo}>
                <Text style={styles.alertSound}>{events[0].sound_name}</Text>
                <Text style={[
                  styles.alertPriority,
                  { color: PRIORITY_COLORS[events[0].priority as keyof typeof PRIORITY_COLORS] },
                ]}>
                  {PRIORITY_LABELS[events[0].priority as keyof typeof PRIORITY_LABELS]}
                </Text>
              </View>
              <Text style={styles.alertDirection}>
                {Math.round(events[0].direction)}°
              </Text>
            </View>
          </View>
        )}

        {/* Recent Events */}
        <Text style={styles.sectionTitle}>Recent Sound Events</Text>
        {events.slice(0, 15).map((event, idx) => (
          <View key={event.event_id} style={styles.eventItem}>
            <Text style={styles.eventIcon}>
              {SOUND_ICONS[event.sound_name] || '🔊'}
            </Text>
            <View style={styles.eventDetails}>
              <Text style={styles.eventSound}>{event.sound_name}</Text>
              <Text style={styles.eventMeta}>
                {event.confidence}% conf • {Math.round(event.direction)}° •
                Node {event.node_id}
              </Text>
            </View>
            <View style={[styles.priorityBadge, {
              backgroundColor: PRIORITY_COLORS[event.priority as keyof typeof PRIORITY_COLORS],
            }]}>
              <Text style={styles.priorityText}>
                {PRIORITY_LABELS[event.priority as keyof typeof PRIORITY_LABELS]}
              </Text>
            </View>
          </View>
        ))}

        {events.length === 0 && (
          <Text style={styles.emptyText}>No sound events yet</Text>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// === Live Events Screen ===

function LiveEventsScreen() {
  const [events, setEvents] = useState<SoundEvent[]>([]);
  const [filter, setFilter] = useState<number | null>(null);

  useEffect(() => {
    fetchEvents();
    const interval = setInterval(fetchEvents, 3000);
    return () => clearInterval(interval);
  }, []);

  const fetchEvents = async () => {
    try {
      const res = await fetch(`${API_BASE}/sound-events?limit=100`);
      const data = await res.json();
      setEvents(filter !== null ? data.filter((e: SoundEvent) => e.priority === filter) : data);
    } catch (e) { /* offline */ }
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Live Events</Text>

      {/* Filter buttons */}
      <View style={styles.filterRow}>
        <TouchableOpacity
          style={[styles.filterBtn, filter === null && styles.filterActive]}
          onPress={() => setFilter(null)}>
          <Text style={styles.filterText}>All</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={[styles.filterBtn, { backgroundColor: '#F44336' }, filter === 2 && styles.filterActive]}
          onPress={() => setFilter(2)}>
          <Text style={styles.filterText}>Emergency</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={[styles.filterBtn, { backgroundColor: '#FF9800' }, filter === 1 && styles.filterActive]}
          onPress={() => setFilter(1)}>
          <Text style={styles.filterText}>Important</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={[styles.filterBtn, { backgroundColor: '#2196F3' }, filter === 0 && styles.filterActive]}
          onPress={() => setFilter(0)}>
          <Text style={styles.filterText}>Info</Text>
        </TouchableOpacity>
      </View>

      <FlatList
        data={events}
        keyExtractor={(item) => item.event_id.toString()}
        renderItem={({ item }) => (
          <View style={styles.eventItem}>
            <Text style={styles.eventIcon}>
              {SOUND_ICONS[item.sound_name] || '🔊'}
            </Text>
            <View style={styles.eventDetails}>
              <Text style={styles.eventSound}>{item.sound_name}</Text>
              <Text style={styles.eventMeta}>
                {new Date(item.timestamp).toLocaleTimeString()} •
                {item.confidence}% • {Math.round(item.direction)}°
              </Text>
            </View>
            <View style={[styles.priorityBadge, {
              backgroundColor: PRIORITY_COLORS[item.priority as keyof typeof PRIORITY_COLORS],
            }]}>
              <Text style={styles.priorityText}>
                {PRIORITY_LABELS[item.priority as keyof typeof PRIORITY_LABELS]}
              </Text>
            </View>
          </View>
        )}
      />
    </SafeAreaView>
  );
}

// === Room Map Screen ===

function RoomMapScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Room Coverage Map</Text>
      <ScrollView>
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Home Floor Plan</Text>
          <Text style={styles.subText}>
            Room sentinels provide directional awareness of sound sources.
            Each sentinel covers a ~10m radius with ±15° direction accuracy.
          </Text>
        </View>

        <View style={styles.mapContainer}>
          <Text style={styles.mapText}>┌─────────────────────────┐</Text>
          <Text style={styles.mapText}>│  Living Room    │ Kitchen     │</Text>
          <Text style={styles.mapText}>│  [Sentinel 1]  │  [Sentinel 2]│</Text>
          <Text style={styles.mapText}>│       ●         │      ●       │</Text>
          <Text style={styles.mapText}>├─────────────────┼─────────────┤</Text>
          <Text style={styles.mapText}>│  Bedroom        │  Office      │</Text>
          <Text style={styles.mapText}>│  [Sentinel 3]  │  [Door Tag]  │</Text>
          <Text style={styles.mapText}>│       ●         │      ●       │</Text>
          <Text style={styles.mapText}>└─────────────────┴─────────────┘</Text>
        </View>

        <View style={styles.legend}>
          <Text style={styles.legendItem}>● Room Sentinel (sound classification)</Text>
          <Text style={styles.legendItem}>● Door Tag (knock/doorbell/phone)</Text>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Coverage Analysis</Text>
          <Text style={styles.subText}>Living Room: ✓ Covered (Sentinel 1)</Text>
          <Text style={styles.subText}>Kitchen: ✓ Covered (Sentinel 2)</Text>
          <Text style={styles.subText}>Bedroom: ✓ Covered (Sentinel 3)</Text>
          <Text style={styles.subText}>Office: ⚠ Partial (Door Tag only)</Text>
          <Text style={styles.subText}>Bathroom: ✗ Not covered</Text>
          <Text style={styles.subText}>Hallway: ✗ Not covered</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// === Wrist Band Screen ===

function WristBandScreen() {
  const [status, setStatus] = useState<WristBandStatus | null>(null);
  const [hapticIntensity, setHapticIntensity] = useState(100);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Wrist Band</Text>
      <ScrollView>
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Status</Text>
          <Text style={styles.subText}>
            Battery: {status?.battery_v?.toFixed(2) || '3.70'}V
          </Text>
          <Text style={styles.subText}>
            Worn: {status?.worn ? '✓ On wrist' : '✗ Not worn'}
          </Text>
          <Text style={styles.subText}>
            Sleep mode: {status?.sleeping ? '😴 Sleeping' : '😊 Awake'}
          </Text>
          <Text style={styles.subText}>
            Alerts today: {status?.alerts_24h || 0}
          </Text>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Haptic Alert Patterns</Text>
          <Text style={styles.subText}>🔴 Emergency: Triple-burst (strong)</Text>
          <Text style={styles.subText}>🟡 Important: Double-pulse (medium)</Text>
          <Text style={styles.subText}>🔵 Info: Single-tap (gentle)</Text>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Haptic Intensity: {hapticIntensity}%</Text>
          <TouchableOpacity
            style={styles.button}
            onPress={() => {
              Vibration.vibrate([0, 500, 150, 500, 150, 500]);
            }}>
            <Text style={styles.buttonText}>Test Emergency Pattern</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.button}
            onPress={() => {
              Vibration.vibrate([0, 200, 100, 200]);
            }}>
            <Text style={styles.buttonText}>Test Important Pattern</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.button}
            onPress={() => {
              Vibration.vibrate(100);
            }}>
            <Text style={styles.buttonText}>Test Info Pattern</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// === Custom Sounds Screen ===

function CustomSoundsScreen() {
  const [enrolling, setEnrolling] = useState(false);
  const [enrollProgress, setEnrollProgress] = useState(0);

  const startEnrollment = () => {
    setEnrolling(true);
    setEnrollProgress(0);
    // Simulate enrollment progress
    const interval = setInterval(() => {
      setEnrollProgress((prev) => {
        if (prev >= 100) {
          clearInterval(interval);
          setEnrolling(false);
          RNAlert.alert('Enrollment Complete', 'Custom sound learned successfully!');
          return 100;
        }
        return prev + 10;
      });
    }, 500);
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Custom Sounds</Text>
      <ScrollView>
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Teach EchoSync Your Sounds</Text>
          <Text style={styles.subText}>
            Record your specific doorbell, phone ring, or alarm sound.
            EchoSync will learn to recognize it with 91% accuracy.
          </Text>
        </View>

        {enrolling ? (
          <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
            <Text style={styles.cardTitle}>Enrolling... {enrollProgress}%</Text>
            <Text style={styles.subText}>
              Play your {enrollProgress < 50 ? 'doorbell' : 'phone ring'} sound
              near a room sentinel...
            </Text>
            <Text style={styles.subText}>
              {'█'.repeat(enrollProgress / 5)}{'░'.repeat(20 - enrollProgress / 5)}
            </Text>
          </View>
        ) : (
          <TouchableOpacity style={styles.button} onPress={startEnrollment}>
            <Text style={styles.buttonText}>Start Enrollment</Text>
          </TouchableOpacity>
        )}

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Enrolled Sounds</Text>
          <Text style={styles.subText}>★ Custom 1: Front doorbell (my house)</Text>
          <Text style={styles.subText}>★ Custom 2: (not enrolled)</Text>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>How It Works</Text>
          <Text style={styles.subText}>
            1. Place a room sentinel near the sound source{'\n'}
            2. Tap "Start Enrollment"{'\n'}
            3. Play the sound for 5 seconds{'\n'}
            4. EchoSync creates a sound prototype{'\n'}
            5. Future detections will match the prototype{'\n'}
            6. Set custom priority (info/important/emergency)
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// === History Screen ===

function HistoryScreen() {
  const [history, setHistory] = useState<any>({});

  useEffect(() => {
    fetchHistory();
  }, []);

  const fetchHistory = async () => {
    try {
      const res = await fetch(`${API_BASE}/sound-events/history?days=7`);
      const data = await res.json();
      setHistory(data);
    } catch (e) { /* offline */ }
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Sound History</Text>
      <ScrollView>
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>7-Day Summary</Text>
          {Object.entries(history).map(([day, stats]: [string, any]) => (
            <View key={day} style={styles.historyDay}>
              <Text style={styles.historyDate}>{day}</Text>
              <Text style={styles.historyStats}>
                Total: {stats.total} | Emergency: {stats.emergency} |
                Important: {stats.important} | Info: {stats.info}
              </Text>
            </View>
          ))}
          {Object.keys(history).length === 0 && (
            <Text style={styles.emptyText}>No history data yet</Text>
          )}
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Weekly Report</Text>
          <Text style={styles.subText}>
            Download accessibility-ready report for sharing with caregivers,
            family members, or healthcare providers.
          </Text>
          <TouchableOpacity
            style={styles.button}
            onPress={() => RNAlert.alert('Report', 'Generating PDF report...')}>
            <Text style={styles.buttonText}>Generate Weekly Report</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// === Alerts Screen ===

function AlertsScreen() {
  const [alerts, setAlerts] = useState<any[]>([]);

  useEffect(() => {
    fetchAlerts();
  }, []);

  const fetchAlerts = async () => {
    try {
      const res = await fetch(`${API_BASE}/alerts`);
      const data = await res.json();
      setAlerts(data);
    } catch (e) { /* offline */ }
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Alerts & Sharing</Text>
      <ScrollView>
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Recent Alerts</Text>
          {alerts.map((alert, idx) => (
            <View key={idx} style={styles.eventItem}>
              <Text style={styles.alertIcon}>
                {alert.type === 'emergency' ? '🚨' : '🔔'}
              </Text>
              <View style={styles.eventDetails}>
                <Text style={styles.eventSound}>{alert.sound}</Text>
                <Text style={styles.eventMeta}>
                  {new Date(alert.timestamp).toLocaleString()}
                </Text>
              </View>
            </View>
          ))}
          {alerts.length === 0 && (
            <Text style={styles.emptyText}>No alerts</Text>
          )}
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Caregiver Sharing</Text>
          <Text style={styles.subText}>
            Share real-time alerts and daily sound logs with family members
            or caregivers. They'll receive push notifications for emergency
            events and a daily summary.
          </Text>
          <TouchableOpacity style={styles.button}
            onPress={() => RNAlert.alert('Share', 'Invite a caregiver...')}>
            <Text style={styles.buttonText}>Invite Caregiver</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// === Settings Screen ===

function SettingsScreen() {
  const [emergencyOnly, setEmergencyOnly] = useState(false);
  const [sleepSuppress, setSleepSuppress] = useState(true);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Settings</Text>
      <ScrollView>
        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Notification Preferences</Text>
          <Text style={styles.subText}>
            Emergency sounds always notify, regardless of settings.
          </Text>
          <TouchableOpacity style={styles.toggleRow}
            onPress={() => setEmergencyOnly(!emergencyOnly)}>
            <Text style={styles.subText}>
              {emergencyOnly ? '☑' : '☐'} Only notify for important+emergency
            </Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.toggleRow}
            onPress={() => setSleepSuppress(!sleepSuppress)}>
            <Text style={styles.subText}>
              {sleepSuppress ? '☑' : '☐'} Suppress non-emergency during sleep
            </Text>
          </TouchableOpacity>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Accessibility Profile</Text>
          <Text style={styles.subText}>Hearing level: Profound deafness</Text>
          <Text style={styles.subText}>Primary alert: Haptic (wrist band)</Text>
          <Text style={styles.subText}>Secondary alert: Visual (LED + display)</Text>
          <Text style={styles.subText}>Tertiary alert: Bed shaker (sleeping)</Text>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>Devices</Text>
          <Text style={styles.subText}>Echo Hub: Online (node 0)</Text>
          <Text style={styles.subText}>Room Sentinel 1: Online (node 1)</Text>
          <Text style={styles.subText}>Room Sentinel 2: Online (node 2)</Text>
          <Text style={styles.subText}>Room Sentinel 3: Online (node 3)</Text>
          <Text style={styles.subText}>Wrist Band: On wrist (node 4)</Text>
          <Text style={styles.subText}>Door Tag 1: Online (node 5)</Text>
          <Text style={styles.subText}>Door Tag 2: Online (node 6)</Text>
        </View>

        <View style={[styles.card, { backgroundColor: '#1a1a2e' }]}>
          <Text style={styles.cardTitle}>About</Text>
          <Text style={styles.subText}>EchoSync v1.0.0</Text>
          <Text style={styles.subText}>
            AI-Powered Sound Awareness System for the Deaf & Hard-of-Hearing
          </Text>
          <Text style={styles.subText}>MIT License — Build it, improve it.</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// === Tab Navigation ===

const Tab = createBottomTabNavigator();

function App() {
  return (
    <NavigationContainer>
      <StatusBar barStyle="light-content" />
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ color, size }) => {
            let iconName: keyof typeof Ionicons.glyphMap = 'home';
            if (route.name === 'Dashboard') iconName = 'home';
            else if (route.name === 'Live') iconName = 'pulse';
            else if (route.name === 'Map') iconName = 'map';
            else if (route.name === 'Wrist') iconName = 'watch';
            else if (route.name === 'Custom') iconName = 'musical-notes';
            else if (route.name === 'History') iconName = 'time';
            else if (route.name === 'Alerts') iconName = 'notifications';
            else if (route.name === 'Settings') iconName = 'settings';
            return <Ionicons name={iconName} size={size} color={color} />;
          },
          tabBarStyle: { backgroundColor: '#0f0f23', borderTopColor: '#333' },
          tabBarActiveTintColor: '#00E5FF',
          tabBarInactiveTintColor: '#666',
          headerShown: false,
        })}>
        <Tab.Screen name="Dashboard" component={DashboardScreen} />
        <Tab.Screen name="Live" component={LiveEventsScreen} />
        <Tab.Screen name="Map" component={RoomMapScreen} />
        <Tab.Screen name="Wrist" component={WristBandScreen} />
        <Tab.Screen name="Custom" component={CustomSoundsScreen} />
        <Tab.Screen name="History" component={HistoryScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
        <Tab.Screen name="Settings" component={SettingsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

// === Styles ===

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f0f23',
    padding: 16,
  },
  header: {
    marginBottom: 16,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#fff',
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 14,
    color: '#888',
  },
  card: {
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
  },
  cardTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#00E5FF',
    marginBottom: 8,
  },
  scoreText: {
    fontSize: 48,
    fontWeight: 'bold',
    color: '#fff',
    textAlign: 'center',
  },
  subText: {
    fontSize: 14,
    color: '#aaa',
    marginBottom: 4,
  },
  alertHeader: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  alertIcon: {
    fontSize: 36,
    marginRight: 12,
  },
  alertInfo: {
    flex: 1,
  },
  alertSound: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#fff',
  },
  alertPriority: {
    fontSize: 14,
    fontWeight: 'bold',
  },
  alertDirection: {
    fontSize: 16,
    color: '#aaa',
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#fff',
    marginBottom: 8,
    marginTop: 8,
  },
  eventItem: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#222',
  },
  eventIcon: {
    fontSize: 24,
    marginRight: 12,
  },
  eventDetails: {
    flex: 1,
  },
  eventSound: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#fff',
  },
  eventMeta: {
    fontSize: 12,
    color: '#888',
  },
  priorityBadge: {
    borderRadius: 6,
    paddingHorizontal: 8,
    paddingVertical: 4,
  },
  priorityText: {
    fontSize: 10,
    fontWeight: 'bold',
    color: '#fff',
  },
  filterRow: {
    flexDirection: 'row',
    marginBottom: 12,
  },
  filterBtn: {
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 8,
    marginRight: 8,
  },
  filterActive: {
    borderWidth: 2,
    borderColor: '#fff',
  },
  filterText: {
    fontSize: 12,
    color: '#fff',
    fontWeight: 'bold',
  },
  mapContainer: {
    backgroundColor: '#000',
    padding: 12,
    borderRadius: 8,
    marginBottom: 12,
  },
  mapText: {
    fontFamily: 'monospace',
    color: '#0F0',
    fontSize: 12,
  },
  legend: {
    marginBottom: 12,
  },
  legendItem: {
    fontSize: 14,
    color: '#aaa',
    marginBottom: 4,
  },
  button: {
    backgroundColor: '#00E5FF',
    borderRadius: 8,
    paddingVertical: 12,
    paddingHorizontal: 24,
    marginTop: 12,
    alignItems: 'center',
  },
  buttonText: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#000',
  },
  historyDay: {
    marginBottom: 8,
  },
  historyDate: {
    fontSize: 14,
    fontWeight: 'bold',
    color: '#fff',
  },
  historyStats: {
    fontSize: 12,
    color: '#aaa',
  },
  emptyText: {
    fontSize: 14,
    color: '#666',
    textAlign: 'center',
    paddingVertical: 20,
  },
  toggleRow: {
    paddingVertical: 8,
  },
});

export default App;