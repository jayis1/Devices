/**
 * HearthKeep Mobile App — React Native
 * 
 * Main app component with bottom tab navigation.
 * Screens: Dashboard, Alerts, Vitals, Activity, Settings, Voice
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  NavigationContainer,
  DarkTheme,
} from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Alert,
  Switch,
  FlatList,
  Dimensions,
} from 'react-native';
import {
  SafeAreaView,
  useSafeAreaInsets,
} from 'react-native-safe-area-context';

// ============================================================================
// TYPES
// ============================================================================

interface RoomStatus {
  nodeId: string;
  roomName: string;
  online: boolean;
  occupied: boolean;
  positionClass: string;
  lastActivity: string;
  temperature: number;
  humidity: number;
  iaq: number;
  lux: number;
}

interface BedVitals {
  heartRate: number;
  breathingRate: number;
  movementIndex: number;
  inBed: boolean;
  sleepPhase: string;
  hrConfidence: number;
  brConfidence: number;
  mattressTemp: number;
}

interface AlertItem {
  id: number;
  type: 'fall' | 'panic' | 'health_trend' | 'routine_change' | 'low_battery';
  severity: 'info' | 'warning' | 'urgent' | 'emergency';
  message: string;
  roomName?: string;
  timestamp: string;
  acknowledged: boolean;
  resolved: boolean;
}

interface WellnessSummary {
  overallStatus: 'normal' | 'alert' | 'emergency';
  rooms: RoomStatus[];
  bed: BedVitals | null;
  alertCount: number;
}

// ============================================================================
// COLORS
// ============================================================================

const Colors = {
  background: '#0A0E1A',
  surface: '#151A2E',
  surfaceLight: '#1E2642',
  primary: '#4A90D9',
  primaryLight: '#6BB3FF',
  success: '#34C759',
  warning: '#FF9500',
  danger: '#FF3B30',
  dangerLight: '#FF6961',
  text: '#FFFFFF',
  textSecondary: '#8E8E93',
  textTertiary: '#636366',
  border: '#2C2C2E',
  accent: '#5AC8FA',
};

// ============================================================================
// MOCK DATA
// ============================================================================

const mockRooms: RoomStatus[] = [
  {
    nodeId: '01',
    roomName: 'Living Room',
    online: true,
    occupied: true,
    positionClass: 'sitting',
    lastActivity: '2 min ago',
    temperature: 22.5,
    humidity: 45,
    iaq: 85,
    lux: 320,
  },
  {
    nodeId: '02',
    roomName: 'Bathroom',
    online: true,
    occupied: false,
    positionClass: 'absent',
    lastActivity: '15 min ago',
    temperature: 24.0,
    humidity: 65,
    iaq: 72,
    lux: 150,
  },
  {
    nodeId: '03',
    roomName: 'Kitchen',
    online: true,
    occupied: true,
    positionClass: 'standing',
    lastActivity: 'Just now',
    temperature: 23.0,
    humidity: 48,
    iaq: 90,
    lux: 450,
  },
  {
    nodeId: '04',
    roomName: 'Bedroom',
    online: true,
    occupied: false,
    positionClass: 'absent',
    lastActivity: '1 hour ago',
    temperature: 21.0,
    humidity: 40,
    iaq: 95,
    lux: 5,
  },
];

const mockBedVitals: BedVitals = {
  heartRate: 68,
  breathingRate: 14,
  movementIndex: 0.05,
  inBed: true,
  sleepPhase: 'deep',
  hrConfidence: 0.92,
  brConfidence: 0.88,
  mattressTemp: 29.2,
};

const mockAlerts: AlertItem[] = [
  {
    id: 1,
    type: 'fall',
    severity: 'emergency',
    message: 'Fall detected in Bathroom',
    roomName: 'Bathroom',
    timestamp: '2 min ago',
    acknowledged: false,
    resolved: false,
  },
  {
    id: 2,
    type: 'routine_change',
    severity: 'warning',
    message: 'Sleeping more than usual — 11 hours vs normal 8',
    timestamp: '3 hours ago',
    acknowledged: false,
    resolved: false,
  },
  {
    id: 3,
    type: 'health_trend',
    severity: 'info',
    message: 'Heart rate trending slightly higher this week (avg 72 vs 68 BPM)',
    timestamp: '1 day ago',
    acknowledged: true,
    resolved: false,
  },
];

// ============================================================================
// SHARED COMPONENTS
// ============================================================================

const StatusBadge = ({ status }: { status: 'normal' | 'alert' | 'emergency' }) => {
  const color = status === 'normal' ? Colors.success 
    : status === 'alert' ? Colors.warning 
    : Colors.danger;
  return (
    <View style={[styles.statusDot, { backgroundColor: color }]} />
  );
};

const SeverityBadge = ({ severity }: { severity: string }) => {
  const color = severity === 'emergency' ? Colors.danger
    : severity === 'urgent' ? Colors.warning
    : severity === 'warning' ? Colors.accent
    : Colors.textSecondary;
  return (
    <View style={[styles.severityBadge, { borderColor: color }]}>
      <Text style={[styles.severityText, { color }]}>{severity.toUpperCase()}</Text>
    </View>
  );
};

// ============================================================================
// DASHBOARD SCREEN
// ============================================================================

const DashboardScreen = () => {
  const [wellness, setWellness] = useState<WellnessSummary>({
    overallStatus: 'normal',
    rooms: mockRooms,
    bed: mockBedVitals,
    alertCount: 2,
  });

  const renderRoomCard = ({ item }: { item: RoomStatus }) => {
    const positionIcon = {
      standing: '🧍',
      sitting: '🪑',
      lying: '🛏️',
      falling: '⚠️',
      fallen: '🆘',
      absent: '—',
    }[item.positionClass] || '?';

    return (
      <View style={styles.roomCard}>
        <View style={styles.roomCardHeader}>
          <Text style={styles.roomCardTitle}>{item.roomName}</Text>
          <StatusBadge status={item.occupied ? 'normal' : 'normal'} />
        </View>
        <View style={styles.roomCardBody}>
          <View style={styles.roomCardRow}>
            <Text style={styles.roomCardIcon}>{positionIcon}</Text>
            <Text style={styles.roomCardLabel}>
              {item.occupied ? item.positionClass : 'Empty'}
            </Text>
          </View>
          <View style={styles.roomCardRow}>
            <Text style={styles.roomCardIcon}>🌡️</Text>
            <Text style={styles.roomCardValue}>{item.temperature}°C</Text>
          </View>
          <View style={styles.roomCardRow}>
            <Text style={styles.roomCardIcon}>💧</Text>
            <Text style={styles.roomCardValue}>{item.humidity}%</Text>
          </View>
          <View style={styles.roomCardRow}>
            <Text style={styles.roomCardIcon}>🏠</Text>
            <Text style={styles.roomCardValue}>IAQ {item.iaq}</Text>
          </View>
        </View>
        <Text style={styles.roomCardFooter}>
          {item.online ? '🟢 Online' : '🔴 Offline'} • {item.lastActivity}
        </Text>
      </View>
    );
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView style={styles.scrollView}>
        {/* Overall Status Banner */}
        <View style={[
          styles.statusBanner,
          wellness.overallStatus === 'emergency' ? styles.bannerEmergency
          : wellness.overallStatus === 'alert' ? styles.bannerAlert
          : styles.bannerNormal,
        ]}>
          <Text style={styles.bannerTitle}>
            {wellness.overallStatus === 'emergency' ? '🚨 EMERGENCY'
              : wellness.overallStatus === 'alert' ? '⚠️ ALERT'
              : '✅ ALL CLEAR'}
          </Text>
          {wellness.alertCount > 0 && (
            <Text style={styles.bannerSubtitle}>
              {wellness.alertCount} active alert{wellness.alertCount > 1 ? 's' : ''}
            </Text>
          )}
        </View>

        {/* Room Status Grid */}
        <Text style={styles.sectionTitle}>Room Status</Text>
        <FlatList
          data={wellness.rooms}
          renderItem={renderRoomCard}
          keyExtractor={(item) => item.nodeId}
          numColumns={2}
          scrollEnabled={false}
          columnWrapperStyle={styles.roomGrid}
        />

        {/* Bed Vitals Card */}
        {wellness.bed && (
          <View style={styles.vitalsCard}>
            <Text style={styles.vitalsCardTitle}>🛏️ Bed Vitals</Text>
            <View style={styles.vitalsGrid}>
              <View style={styles.vitalItem}>
                <Text style={styles.vitalIcon}>❤️</Text>
                <Text style={styles.vitalValue}>{wellness.bed.heartRate.toFixed(0)}</Text>
                <Text style={styles.vitalUnit}>BPM</Text>
                <Text style={styles.vitalConfidence}>
                  {wellness.bed.hrConfidence.toFixed(0)}% conf.
                </Text>
              </View>
              <View style={styles.vitalItem}>
                <Text style={styles.vitalIcon}>🫁</Text>
                <Text style={styles.vitalValue}>{wellness.bed.breathingRate.toFixed(0)}</Text>
                <Text style={styles.vitalUnit}>breaths/min</Text>
                <Text style={styles.vitalConfidence}>
                  {wellness.bed.brConfidence.toFixed(0)}% conf.
                </Text>
              </View>
              <View style={styles.vitalItem}>
                <Text style={styles.vitalIcon}>😴</Text>
                <Text style={styles.vitalValue}>
                  {wellness.bed.sleepPhase === 'deep' ? 'Deep' 
                    : wellness.bed.sleepPhase === 'light' ? 'Light'
                    : wellness.bed.sleepPhase === 'rem' ? 'REM' 
                    : wellness.bed.sleepPhase === 'awake' ? 'Awake' : '—'}
                </Text>
                <Text style={styles.vitalUnit}>Sleep Phase</Text>
              </View>
              <View style={styles.vitalItem}>
                <Text style={styles.vitalIcon}>🏃</Text>
                <Text style={styles.vitalValue}>
                  {(wellness.bed.movementIndex * 100).toFixed(0)}
                </Text>
                <Text style={styles.vitalUnit}>Movement %</Text>
              </View>
            </View>
          </View>
        )}
      </ScrollView>
    </SafeAreaView>
  );
};

// ============================================================================
// ALERTS SCREEN
// ============================================================================

const AlertsScreen = () => {
  const [alerts, setAlerts] = useState<AlertItem[]>(mockAlerts);

  const acknowledgeAlert = (id: number) => {
    setAlerts(alerts.map(a => 
      a.id === id ? { ...a, acknowledged: true } : a
    ));
  };

  const resolveAlert = (id: number) => {
    setAlerts(alerts.map(a => 
      a.id === id ? { ...a, resolved: true, acknowledged: true } : a
    ));
  };

  const renderAlert = ({ item }: { item: AlertItem }) => {
    if (item.resolved) return null;
    
    return (
      <View style={[
        styles.alertCard,
        item.severity === 'emergency' ? styles.alertEmergency
        : item.severity === 'urgent' ? styles.alertUrgent
        : item.severity === 'warning' ? styles.alertWarning
        : styles.alertInfo,
      ]}>
        <View style={styles.alertHeader}>
          <SeverityBadge severity={item.severity} />
          <Text style={styles.alertTimestamp}>{item.timestamp}</Text>
        </View>
        <Text style={styles.alertMessage}>{item.message}</Text>
        {item.roomName && (
          <Text style={styles.alertRoom}>📍 {item.roomName}</Text>
        )}
        <View style={styles.alertActions}>
          {!item.acknowledged && (
            <TouchableOpacity 
              style={styles.alertButton}
              onPress={() => acknowledgeAlert(item.id)}>
              <Text style={styles.alertButtonText}>Acknowledge</Text>
            </TouchableOpacity>
          )}
          <TouchableOpacity 
            style={[styles.alertButton, styles.alertButtonResolve]}
            onPress={() => resolveAlert(item.id)}>
            <Text style={styles.alertButtonText}>Resolve</Text>
          </TouchableOpacity>
        </View>
      </View>
    );
  };

  const activeAlerts = alerts.filter(a => !a.resolved);
  const resolvedAlerts = alerts.filter(a => a.resolved);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.sectionTitle}>Active Alerts ({activeAlerts.length})</Text>
      <FlatList
        data={activeAlerts}
        renderItem={renderAlert}
        keyExtractor={(item) => item.id.toString()}
        contentContainerStyle={styles.alertList}
      />
      
      {resolvedAlerts.length > 0 && (
        <>
          <Text style={styles.sectionTitle}>Resolved ({resolvedAlerts.length})</Text>
          <FlatList
            data={resolvedAlerts}
            renderItem={({ item }) => (
              <View style={[styles.alertCard, styles.alertResolved]}>
                <Text style={[styles.alertMessage, { opacity: 0.5 }]}>
                  {item.message}
                </Text>
                <Text style={styles.alertTimestamp}>{item.timestamp} — Resolved ✓</Text>
              </View>
            )}
            keyExtractor={(item) => item.id.toString()}
            contentContainerStyle={styles.alertList}
          />
        </>
      )}
    </SafeAreaView>
  );
};

// ============================================================================
// VITALS SCREEN
// ============================================================================

const VitalsScreen = () => {
  return (
    <SafeAreaView style={styles.container}>
      <ScrollView style={styles.scrollView}>
        <Text style={styles.sectionTitle}>Heart Rate</Text>
        <View style={styles.chartPlaceholder}>
          <Text style={styles.chartPlaceholderText}>
            📊 Heart rate over last 24 hours{'\n'}
            Current: 68 BPM • Avg: 72 BPM
          </Text>
        </View>

        <Text style={styles.sectionTitle}>Breathing Rate</Text>
        <View style={styles.chartPlaceholder}>
          <Text style={styles.chartPlaceholderText}>
            📊 Breathing rate over last 24 hours{'\n'}
            Current: 14 breaths/min • Avg: 15 breaths/min
          </Text>
        </View>

        <Text style={styles.sectionTitle}>Sleep Quality</Text>
        <View style={styles.chartPlaceholder}>
          <Text style={styles.chartPlaceholderText}>
            📊 Sleep phases over last 12 hours{'\n'}
            Deep: 3.2h • Light: 2.8h • REM: 1.5h • Awake: 0.5h
          </Text>
        </View>

        <Text style={styles.sectionTitle}>Movement Index</Text>
        <View style={styles.chartPlaceholder}>
          <Text style={styles.chartPlaceholderText}>
            📊 Movement level over last 24 hours{'\n'}
            Daily activity: Moderate
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
};

// ============================================================================
// ACTIVITY SCREEN
// ============================================================================

const ActivityScreen = () => {
  const hours = Array.from({ length: 24 }, (_, i) => i);
  const activityColors: Record<string, string> = {
    sleeping: Colors.primary,
    sitting: Colors.accent,
    standing: Colors.success,
    walking: Colors.warning,
    cooking: Colors.danger,
    bathroom: Colors.primaryLight,
    absent: Colors.textTertiary,
  };

  // Mock activity timeline
  const timeline = [
    { hour: 0, activity: 'sleeping' },
    { hour: 1, activity: 'sleeping' },
    { hour: 2, activity: 'sleeping' },
    { hour: 3, activity: 'sleeping' },
    { hour: 4, activity: 'sleeping' },
    { hour: 5, activity: 'sleeping' },
    { hour: 6, activity: 'sleeping' },
    { hour: 7, activity: 'sitting' },
    { hour: 8, activity: 'cooking' },
    { hour: 9, activity: 'sitting' },
    { hour: 10, activity: 'walking' },
    { hour: 11, activity: 'sitting' },
    { hour: 12, activity: 'cooking' },
    { hour: 13, activity: 'sleeping' },
    { hour: 14, activity: 'sitting' },
    { hour: 15, activity: 'sitting' },
    { hour: 16, activity: 'walking' },
    { hour: 17, activity: 'sitting' },
    { hour: 18, activity: 'cooking' },
    { hour: 19, activity: 'sitting' },
    { hour: 20, activity: 'bathroom' },
    { hour: 21, activity: 'sitting' },
    { hour: 22, activity: 'sleeping' },
    { hour: 23, activity: 'sleeping' },
  ];

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView style={styles.scrollView}>
        <Text style={styles.sectionTitle}>Daily Activity Timeline</Text>
        
        {/* Activity bars */}
        <View style={styles.timelineContainer}>
          {timeline.map((item, idx) => (
            <View key={idx} style={styles.timelineRow}>
              <Text style={styles.timelineHour}>
                {item.hour.toString().padStart(2, '0')}:00
              </Text>
              <View style={[
                styles.timelineBar,
                { backgroundColor: activityColors[item.activity] || Colors.textTertiary }
              ]} />
              <Text style={styles.timelineActivity}>
                {item.activity}
              </Text>
            </View>
          ))}
        </View>

        {/* Legend */}
        <View style={styles.legendContainer}>
          {Object.entries(activityColors).map(([activity, color]) => (
            <View key={activity} style={styles.legendItem}>
              <View style={[styles.legendDot, { backgroundColor: color }]} />
              <Text style={styles.legendText}>{activity}</Text>
            </View>
          ))}
        </View>

        {/* Activity summary */}
        <View style={styles.vitalsCard}>
          <Text style={styles.vitalsCardTitle}>Daily Summary</Text>
          <View style={styles.vitalsGrid}>
            <View style={styles.vitalItem}>
              <Text style={styles.vitalIcon}>🛏️</Text>
              <Text style={styles.vitalValue}>8.2</Text>
              <Text style={styles.vitalUnit}>hours sleep</Text>
            </View>
            <View style={styles.vitalItem}>
              <Text style={styles.vitalIcon}>🚶</Text>
              <Text style={styles.vitalValue}>2,450</Text>
              <Text style={styles.vitalUnit}>steps equiv.</Text>
            </View>
            <View style={styles.vitalItem}>
              <Text style={styles.vitalIcon}>🚿</Text>
              <Text style={styles.vitalValue}>3</Text>
              <Text style={styles.vitalUnit}>bathroom visits</Text>
            </View>
            <View style={styles.vitalItem}>
              <Text style={styles.vitalIcon}>🍳</Text>
              <Text style={styles.vitalValue}>3</Text>
              <Text style={styles.vitalUnit}>kitchen visits</Text>
            </View>
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
};

// ============================================================================
// SETTINGS SCREEN
// ============================================================================

const SettingsScreen = () => {
  const [sensitivity, setSensitivity] = useState('normal');
  const [alertEscalation, setAlertEscalation] = useState(true);
  const [voicePrompts, setVoicePrompts] = useState(true);
  const [nightMode, setNightMode] = useState(true);
  const [privacyMode, setPrivacyMode] = useState(false);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView style={styles.scrollView}>
        <Text style={styles.sectionTitle}>Fall Detection</Text>
        <View style={styles.settingsCard}>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Sensitivity</Text>
            <View style={styles.settingsOptions}>
              {['low', 'normal', 'high'].map((level) => (
                <TouchableOpacity
                  key={level}
                  style={[
                    styles.settingsOption,
                    sensitivity === level && styles.settingsOptionActive,
                  ]}
                  onPress={() => setSensitivity(level)}>
                  <Text style={[
                    styles.settingsOptionText,
                    sensitivity === level && styles.settingsOptionTextActive,
                  ]}>
                    {level.charAt(0).toUpperCase() + level.slice(1)}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Alert Escalation</Text>
            <Switch
              value={alertEscalation}
              onValueChange={setAlertEscalation}
              trackColor={{ false: Colors.border, true: Colors.primary }}
            />
          </View>
        </View>

        <Text style={styles.sectionTitle}>Hub Settings</Text>
        <View style={styles.settingsCard}>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Voice Prompts</Text>
            <Switch
              value={voicePrompts}
              onValueChange={setVoicePrompts}
              trackColor={{ false: Colors.border, true: Colors.primary }}
            />
          </View>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Night Mode (quiet after 10pm)</Text>
            <Switch
              value={nightMode}
              onValueChange={setNightMode}
              trackColor={{ false: Colors.border, true: Colors.primary }}
            />
          </View>
        </View>

        <Text style={styles.sectionTitle}>Privacy</Text>
        <View style={styles.settingsCard}>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Privacy Mode</Text>
            <Text style={styles.settingsDescription}>
              Stops all data upload to cloud. Local fall detection still works.
            </Text>
            <Switch
              value={privacyMode}
              onValueChange={setPrivacyMode}
              trackColor={{ false: Colors.border, true: Colors.primary }}
            />
          </View>
        </View>

        <Text style={styles.sectionTitle}>Emergency Contacts</Text>
        <View style={styles.settingsCard}>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Primary Contact</Text>
            <Text style={styles.settingsValue}>Sarah Johnson</Text>
          </View>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Secondary Contact</Text>
            <Text style={styles.settingsValue}>Dr. Michael Chen</Text>
          </View>
          <TouchableOpacity style={styles.addButton}>
            <Text style={styles.addButtonText}>+ Add Contact</Text>
          </TouchableOpacity>
        </View>

        <Text style={styles.sectionTitle}>System</Text>
        <View style={styles.settingsCard}>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Hub Firmware</Text>
            <Text style={styles.settingsValue}>v1.2.0</Text>
          </View>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>Connected Nodes</Text>
            <Text style={styles.settingsValue}>6 (4 rooms + 1 bed + 1 tag)</Text>
          </View>
          <View style={styles.settingsRow}>
            <Text style={styles.settingsLabel}>WiFi Network</Text>
            <Text style={styles.settingsValue}>Home_WiFi_5G</Text>
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
};

// ============================================================================
// NAVIGATION
// ============================================================================

const Tab = createBottomTabNavigator();

const App = () => {
  return (
    <NavigationContainer theme={DarkTheme}>
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: Colors.primary,
          tabBarInactiveTintColor: Colors.textSecondary,
          tabBarStyle: {
            backgroundColor: Colors.surface,
            borderTopColor: Colors.border,
          },
          headerStyle: {
            backgroundColor: Colors.background,
          },
          headerTintColor: Colors.text,
        }}
      >
        <Tab.Screen
          name="Dashboard"
          component={DashboardScreen}
          options={{
            tabBarLabel: 'Home',
            tabBarIcon: ({ color }) => <Text style={{ color }}>🏠</Text>,
          }}
        />
        <Tab.Screen
          name="Alerts"
          component={AlertsScreen}
          options={{
            tabBarLabel: 'Alerts',
            tabBarIcon: ({ color }) => <Text style={{ color }}>🔔</Text>,
            tabBarBadge: 2,
          }}
        />
        <Tab.Screen
          name="Vitals"
          component={VitalsScreen}
          options={{
            tabBarLabel: 'Vitals',
            tabBarIcon: ({ color }) => <Text style={{ color }}>❤️</Text>,
          }}
        />
        <Tab.Screen
          name="Activity"
          component={ActivityScreen}
          options={{
            tabBarLabel: 'Activity',
            tabBarIcon: ({ color }) => <Text style={{ color }}>📊</Text>,
          }}
        />
        <Tab.Screen
          name="Settings"
          component={SettingsScreen}
          options={{
            tabBarLabel: 'Settings',
            tabBarIcon: ({ color }) => <Text style={{ color }}>⚙️</Text>,
          }}
        />
      </Tab.Navigator>
    </NavigationContainer>
  );
};

// ============================================================================
// STYLES
// ============================================================================

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: Colors.background,
  },
  scrollView: {
    flex: 1,
    padding: 16,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '700',
    color: Colors.text,
    marginTop: 16,
    marginBottom: 8,
  },
  statusBanner: {
    padding: 16,
    borderRadius: 12,
    marginBottom: 16,
  },
  bannerNormal: {
    backgroundColor: '#1a3a1a',
    borderWidth: 1,
    borderColor: Colors.success,
  },
  bannerAlert: {
    backgroundColor: '#3a2a0a',
    borderWidth: 1,
    borderColor: Colors.warning,
  },
  bannerEmergency: {
    backgroundColor: '#3a0a0a',
    borderWidth: 1,
    borderColor: Colors.danger,
  },
  bannerTitle: {
    fontSize: 20,
    fontWeight: '700',
    color: Colors.text,
  },
  bannerSubtitle: {
    fontSize: 14,
    color: Colors.textSecondary,
    marginTop: 4,
  },
  roomGrid: {
    justifyContent: 'space-between',
  },
  roomCard: {
    flex: 1,
    backgroundColor: Colors.surface,
    borderRadius: 12,
    padding: 12,
    margin: 4,
    borderWidth: 1,
    borderColor: Colors.border,
  },
  roomCardHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  roomCardTitle: {
    fontSize: 14,
    fontWeight: '600',
    color: Colors.text,
  },
  roomCardBody: {
    marginBottom: 8,
  },
  roomCardRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 2,
  },
  roomCardIcon: {
    fontSize: 14,
    width: 20,
  },
  roomCardLabel: {
    fontSize: 12,
    color: Colors.textSecondary,
  },
  roomCardValue: {
    fontSize: 12,
    color: Colors.textSecondary,
  },
  roomCardFooter: {
    fontSize: 10,
    color: Colors.textTertiary,
  },
  statusDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  vitalsCard: {
    backgroundColor: Colors.surface,
    borderRadius: 12,
    padding: 16,
    marginTop: 8,
    borderWidth: 1,
    borderColor: Colors.border,
  },
  vitalsCardTitle: {
    fontSize: 16,
    fontWeight: '700',
    color: Colors.text,
    marginBottom: 12,
  },
  vitalsGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
  },
  vitalItem: {
    width: '48%',
    alignItems: 'center',
    marginBottom: 12,
  },
  vitalIcon: {
    fontSize: 24,
    marginBottom: 4,
  },
  vitalValue: {
    fontSize: 28,
    fontWeight: '700',
    color: Colors.text,
  },
  vitalUnit: {
    fontSize: 12,
    color: Colors.textSecondary,
  },
  vitalConfidence: {
    fontSize: 10,
    color: Colors.textTertiary,
  },
  alertCard: {
    backgroundColor: Colors.surface,
    borderRadius: 12,
    padding: 16,
    marginTop: 8,
    borderWidth: 1,
  },
  alertEmergency: {
    borderColor: Colors.danger,
    backgroundColor: '#2a0a0a',
  },
  alertUrgent: {
    borderColor: Colors.warning,
    backgroundColor: '#2a1a0a',
  },
  alertWarning: {
    borderColor: Colors.accent,
    backgroundColor: '#0a1a2a',
  },
  alertInfo: {
    borderColor: Colors.textTertiary,
  },
  alertResolved: {
    opacity: 0.5,
  },
  alertHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  alertTimestamp: {
    fontSize: 12,
    color: Colors.textTertiary,
  },
  alertMessage: {
    fontSize: 14,
    color: Colors.text,
    fontWeight: '500',
  },
  alertRoom: {
    fontSize: 12,
    color: Colors.textSecondary,
    marginTop: 4,
  },
  alertActions: {
    flexDirection: 'row',
    justifyContent: 'flex-end',
    marginTop: 12,
  },
  alertButton: {
    backgroundColor: Colors.primary,
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
    marginLeft: 8,
  },
  alertButtonResolve: {
    backgroundColor: Colors.success,
  },
  alertButtonText: {
    color: Colors.text,
    fontSize: 13,
    fontWeight: '600',
  },
  alertList: {
    paddingBottom: 16,
  },
  severityBadge: {
    borderWidth: 1,
    borderRadius: 4,
    paddingHorizontal: 6,
    paddingVertical: 2,
  },
  severityText: {
    fontSize: 10,
    fontWeight: '700',
  },
  chartPlaceholder: {
    backgroundColor: Colors.surface,
    borderRadius: 12,
    padding: 24,
    alignItems: 'center',
    borderWidth: 1,
    borderColor: Colors.border,
  },
  chartPlaceholderText: {
    color: Colors.textSecondary,
    fontSize: 14,
    textAlign: 'center',
    lineHeight: 20,
  },
  timelineContainer: {
    backgroundColor: Colors.surface,
    borderRadius: 12,
    padding: 12,
    borderWidth: 1,
    borderColor: Colors.border,
  },
  timelineRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 4,
  },
  timelineHour: {
    fontSize: 10,
    color: Colors.textTertiary,
    width: 40,
    fontFamily: 'monospace',
  },
  timelineBar: {
    flex: 1,
    height: 16,
    borderRadius: 3,
    marginHorizontal: 8,
  },
  timelineActivity: {
    fontSize: 10,
    color: Colors.textSecondary,
    width: 70,
    textTransform: 'capitalize',
  },
  legendContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    marginTop: 8,
    marginBottom: 16,
  },
  legendItem: {
    flexDirection: 'row',
    alignItems: 'center',
    marginRight: 16,
    marginBottom: 4,
  },
  legendDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
    marginRight: 4,
  },
  legendText: {
    fontSize: 12,
    color: Colors.textSecondary,
    textTransform: 'capitalize',
  },
  settingsCard: {
    backgroundColor: Colors.surface,
    borderRadius: 12,
    padding: 16,
    borderWidth: 1,
    borderColor: Colors.border,
  },
  settingsRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: Colors.border,
  },
  settingsLabel: {
    fontSize: 14,
    color: Colors.text,
    fontWeight: '500',
  },
  settingsDescription: {
    fontSize: 12,
    color: Colors.textSecondary,
    marginTop: 4,
  },
  settingsValue: {
    fontSize: 14,
    color: Colors.textSecondary,
  },
  settingsOptions: {
    flexDirection: 'row',
  },
  settingsOption: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 6,
    marginHorizontal: 4,
    backgroundColor: Colors.surfaceLight,
  },
  settingsOptionActive: {
    backgroundColor: Colors.primary,
  },
  settingsOptionText: {
    fontSize: 12,
    color: Colors.textSecondary,
  },
  settingsOptionTextActive: {
    color: Colors.text,
    fontWeight: '600',
  },
  addButton: {
    paddingVertical: 12,
    alignItems: 'center',
    marginTop: 8,
  },
  addButtonText: {
    color: Colors.primary,
    fontSize: 14,
    fontWeight: '600',
  },
});

export default App;