import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  RefreshControl,
  Alert,
} from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';

// ========== API CONFIGURATION ==========
const API_BASE = 'http://breathhome.local:8000/api';
const WS_URL = 'ws://breathhome.local:8000/ws/realtime';

// ========== TYPES ==========
interface RoomAQI {
  room_id: number;
  room_name: string;
  aqi_score: number;
  aqi_category: number;
  pm2_5: number;
  co2: number;
  voc_index: number;
  temperature: number;
  humidity: number;
  mold_risk: number;
  timestamp: string;
}

interface AlertEvent {
  id: number;
  room_id: number;
  parameter: string;
  value: number;
  threshold: number;
  category: string;
  message: string;
  timestamp: string;
  acknowledged: boolean;
}

interface ExposureData {
  tag_id: number;
  timestamp: string;
  eco2: number;
  tvoc: number;
  personal_aqi: number;
  activity: number;
  symptom_flag: number;
  battery_pct: number;
}

// ========== UTILITY FUNCTIONS ==========

function getAQIColor(aqi: number): string {
  if (aqi <= 50) return '#4CAF50';      // Green
  if (aqi <= 100) return '#FFEB3B';     // Yellow
  if (aqi <= 150) return '#FF9800';     // Orange
  if (aqi <= 200) return '#F44336';     // Red
  if (aqi <= 300) return '#9C27B0';     // Purple
  return '#7E0023';                      // Maroon
}

function getAQILabel(category: number): string {
  const labels = ['Good', 'Moderate', 'Unhealthy (Sensitive)', 'Unhealthy', 'Very Unhealthy', 'Hazardous'];
  return labels[category] || 'Unknown';
}

function getAQIEmoji(category: number): string {
  const emojis = ['😊', '😐', '😷', '🤢', '🤮', '☠️'];
  return emojis[category] || '❓';
}

function formatTime(timestamp: string): string {
  const date = new Date(timestamp);
  return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

// ========== AQI GAUGE COMPONENT ==========

function AQIGauge({ value, size = 120 }: { value: number; size?: number }) {
  const color = getAQIColor(value);
  const percentage = Math.min(value / 300, 1);
  
  return (
    <View style={[styles.gaugeContainer, { width: size, height: size }]}>
      <View style={[styles.gaugeOuter, { 
        borderColor: color, 
        width: size, 
        height: size,
        borderRadius: size / 2 
      }]}>
        <View style={styles.gaugeInner}>
          <Text style={[styles.gaugeValue, { color }]}>{Math.round(value)}</Text>
          <Text style={styles.gaugeLabel}>AQI</Text>
        </View>
      </View>
    </View>
  );
}

// ========== HOME SCREEN ==========

function HomeScreen() {
  const [rooms, setRooms] = useState<RoomAQI[]>([]);
  const [alerts, setAlerts] = useState<AlertEvent[]>([]);
  const [refreshing, setRefreshing] = useState(false);
  const [connected, setConnected] = useState(false);

  const fetchRooms = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/rooms`);
      const data = await response.json();
      setRooms(data);
    } catch (error) {
      console.error('Failed to fetch rooms:', error);
    }
  }, []);

  const fetchAlerts = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/alerts?limit=10`);
      const data = await response.json();
      setAlerts(data);
    } catch (error) {
      console.error('Failed to fetch alerts:', error);
    }
  }, []);

  useEffect(() => {
    fetchRooms();
    fetchAlerts();
    const interval = setInterval(() => {
      fetchRooms();
      fetchAlerts();
    }, 30000);
    return () => clearInterval(interval);
  }, [fetchRooms, fetchAlerts]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await Promise.all([fetchRooms(), fetchAlerts()]);
    setRefreshing(false);
  }, [fetchRooms, fetchAlerts]);

  // Find worst AQI
  const worstAQI = rooms.length > 0 ? Math.max(...rooms.map(r => r.aqi_score || 0)) : 0;
  const worstCategory = rooms.length > 0 
    ? Math.max(...rooms.map(r => r.aqi_category || 0)) 
    : 0;

  return (
    <ScrollView 
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.headerTitle}>BreathHome</Text>
        <Text style={styles.headerSubtitle}>
          {connected ? '🟢 Connected' : '🔴 Disconnected'}
        </Text>
      </View>

      {/* Overall AQI */}
      <View style={styles.overallCard}>
        <Text style={styles.cardTitle}>Home Air Quality</Text>
        <View style={styles.overallRow}>
          <AQIGauge value={worstAQI} size={140} />
          <View style={styles.overallInfo}>
            <Text style={[styles.aqiCategory, { color: getAQIColor(worstAQI) }]}>
              {getAQILabel(worstCategory)}
            </Text>
            <Text style={styles.aqiEmoji}>{getAQIEmoji(worstCategory)}</Text>
            <Text style={styles.aqiNote}>
              {worstAQI <= 50 ? 'Air is clean everywhere!' :
               worstAQI <= 100 ? 'Air is acceptable' :
               worstAQI <= 150 ? 'Sensitive groups should take care' :
               'Ventilation recommended'}
            </Text>
          </View>
        </View>
      </View>

      {/* Room Cards */}
      <Text style={styles.sectionTitle}>Rooms</Text>
      {rooms.map((room) => (
        <View key={room.room_id} style={styles.roomCard}>
          <View style={styles.roomHeader}>
            <View style={[styles.aqiBadge, { backgroundColor: getAQIColor(room.aqi_score || 0) }]}>
              <Text style={styles.aqiBadgeText}>{Math.round(room.aqi_score || 0)}</Text>
            </View>
            <View style={styles.roomInfo}>
              <Text style={styles.roomName}>Room {room.room_id}</Text>
              <Text style={styles.roomStatus}>{getAQILabel(room.aqi_category || 0)}</Text>
            </View>
            <Text style={styles.roomTime}>{formatTime(room.timestamp)}</Text>
          </View>
          <View style={styles.roomMetrics}>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{(room.pm2_5 || 0).toFixed(1)}</Text>
              <Text style={styles.metricLabel}>PM2.5</Text>
            </View>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{Math.round(room.co2 || 0)}</Text>
              <Text style={styles.metricLabel}>CO₂</Text>
            </View>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{Math.round(room.voc_index || 0)}</Text>
              <Text style={styles.metricLabel}>VOC</Text>
            </View>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{(room.temperature || 0).toFixed(1)}°</Text>
              <Text style={styles.metricLabel}>Temp</Text>
            </View>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{Math.round(room.humidity || 0)}%</Text>
              <Text style={styles.metricLabel}>Humidity</Text>
            </View>
          </View>
          {(room.mold_risk || 0) > 60 && (
            <View style={styles.moldWarning}>
              <Text style={styles.moldWarningText}>
                🦠 Mold risk: {Math.round(room.mold_risk || 0)}%
              </Text>
            </View>
          )}
        </View>
      ))}

      {/* Active Alerts */}
      {alerts.filter(a => !a.acknowledged).length > 0 && (
        <>
          <Text style={styles.sectionTitle}>⚠️ Active Alerts</Text>
          {alerts.filter(a => !a.acknowledged).slice(0, 5).map((alert) => (
            <View key={alert.id} style={[
              styles.alertCard,
              alert.category === 'critical' && styles.alertCritical,
              alert.category === 'danger' && styles.alertDanger,
              alert.category === 'warning' && styles.alertWarning,
            ]}>
              <Text style={styles.alertMessage}>{alert.message}</Text>
              <Text style={styles.alertTime}>{formatTime(alert.timestamp)}</Text>
            </View>
          ))}
        </>
      )}
    </ScrollView>
  );
}

// ========== EXPOSURE SCREEN ==========

function ExposureScreen() {
  const [exposure, setExposure] = useState<ExposureData[]>([]);
  const [tagId, setTagId] = useState(1);

  useEffect(() => {
    const fetchExposure = async () => {
      try {
        const response = await fetch(`${API_BASE}/exposure/${tagId}?hours=24`);
        const data = await response.json();
        setExposure(data);
      } catch (error) {
        console.error('Failed to fetch exposure:', error);
      }
    };
    fetchExposure();
    const interval = setInterval(fetchExposure, 60000);
    return () => clearInterval(interval);
  }, [tagId]);

  const currentExposure = exposure.length > 0 ? exposure[exposure.length - 1] : null;
  const avgAqi = exposure.length > 0 
    ? exposure.reduce((sum, e) => sum + (e.personal_aqi || 0), 0) / exposure.length 
    : 0;
  const peakAqi = exposure.length > 0 
    ? Math.max(...exposure.map(e => e.personal_aqi || 0)) 
    : 0;

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>My Exposure</Text>
      </View>

      {currentExposure && (
        <View style={styles.exposureCard}>
          <Text style={styles.cardTitle}>Current Personal Air Quality</Text>
          <AQIGauge value={currentExposure.personal_aqi || 0} size={160} />
          <View style={styles.exposureMetrics}>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{Math.round(currentExposure.eco2 || 0)}</Text>
              <Text style={styles.metricLabel}>eCO₂ (ppm)</Text>
            </View>
            <View style={styles.metric}>
              <Text style={styles.metricValue}>{Math.round(currentExposure.tvoc || 0)}</Text>
              <Text style={styles.metricLabel}>TVOC (ppb)</Text>
            </View>
          </View>
          <View style={styles.exposureStats}>
            <View style={styles.statBox}>
              <Text style={styles.statValue}>{Math.round(avgAqi)}</Text>
              <Text style={styles.statLabel}>24h Avg AQI</Text>
            </View>
            <View style={styles.statBox}>
              <Text style={styles.statValue}>{Math.round(peakAqi)}</Text>
              <Text style={styles.statLabel}>24h Peak AQI</Text>
            </View>
            <View style={styles.statBox}>
              <Text style={styles.statValue}>{currentExposure.battery_pct || 0}%</Text>
              <Text style={styles.statLabel}>Battery</Text>
            </View>
          </View>
          <Text style={styles.activityLabel}>
            Activity: {['Still', 'Walking', 'Running', 'Sleeping'][currentExposure.activity || 0]}
          </Text>
          {currentExposure.symptom_flag > 0 && (
            <View style={styles.symptomBanner}>
              <Text style={styles.symptomText}>
                Symptom logged: {
                  ['None', 'Wheeze', 'Cough', 'Shortness of breath', 'Throat irritation'
                ][currentExposure.symptom_flag] || 'Unknown'}
              </Text>
            </View>
          )}
        </View>
      )}
    </ScrollView>
  );
}

// ========== ALERTS SCREEN ==========

function AlertsScreen() {
  const [alerts, setAlerts] = useState<AlertEvent[]>([]);
  const [filter, setFilter] = useState<string>('all');

  useEffect(() => {
    const fetchAlerts = async () => {
      try {
        const response = await fetch(`${API_BASE}/alerts?limit=50`);
        const data = await response.json();
        setAlerts(data);
      } catch (error) {
        console.error('Failed to fetch alerts:', error);
      }
    };
    fetchAlerts();
    const interval = setInterval(fetchAlerts, 30000);
    return () => clearInterval(interval);
  }, []);

  const acknowledgeAlert = async (id: number) => {
    try {
      await fetch(`${API_BASE}/alerts/${id}/acknowledge`, { method: 'PUT' });
      setAlerts(alerts.map(a => a.id === id ? { ...a, acknowledged: true } : a));
    } catch (error) {
      console.error('Failed to acknowledge alert:', error);
    }
  };

  const filteredAlerts = filter === 'all' ? alerts : alerts.filter(a => a.category === filter);

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Alerts</Text>
      </View>
      
      {/* Filter tabs */}
      <View style={styles.filterRow}>
        {['all', 'critical', 'danger', 'warning', 'info'].map(f => (
          <TouchableOpacity 
            key={f} 
            style={[styles.filterTab, filter === f && styles.filterActive]}
            onPress={() => setFilter(f)}
          >
            <Text style={[styles.filterText, filter === f && styles.filterTextActive]}>
              {f.charAt(0).toUpperCase() + f.slice(1)}
            </Text>
          </TouchableOpacity>
        ))}
      </View>

      {filteredAlerts.map(alert => (
        <View key={alert.id} style={[
          styles.alertCard,
          alert.category === 'critical' && styles.alertCritical,
          alert.category === 'danger' && styles.alertDanger,
          alert.category === 'warning' && styles.alertWarning,
          alert.acknowledged && styles.alertAcknowledged,
        ]}>
          <Text style={styles.alertMessage}>{alert.message}</Text>
          <View style={styles.alertFooter}>
            <Text style={styles.alertTime}>
              {new Date(alert.timestamp).toLocaleString()}
            </Text>
            {!alert.acknowledged && (
              <TouchableOpacity 
                style={styles.ackButton}
                onPress={() => acknowledgeAlert(alert.id)}
              >
                <Text style={styles.ackButtonText}>Acknowledge</Text>
              </TouchableOpacity>
            )}
          </View>
        </View>
      ))}
    </ScrollView>
  );
}

// ========== HVAC CONTROL SCREEN ==========

function HVACControlScreen() {
  const [hvacState, setHvacState] = useState<any>(null);

  useEffect(() => {
    const fetchHVAC = async () => {
      try {
        const response = await fetch(`${API_BASE}/hvac/status`);
        const data = await response.json();
        setHvacState(data);
      } catch (error) {
        console.error('Failed to fetch HVAC state:', error);
      }
    };
    fetchHVAC();
    const interval = setInterval(fetchHVAC, 10000);
    return () => clearInterval(interval);
  }, []);

  const sendCommand = async (command: object) => {
    try {
      await fetch(`${API_BASE}/hvac/command`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(command),
      });
    } catch (error) {
      console.error('Failed to send HVAC command:', error);
    }
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>HVAC Control</Text>
      </View>

      {hvacState && (
        <>
          {/* Filter Health */}
          <View style={styles.card}>
            <Text style={styles.cardTitle}>🔧 Filter Health</Text>
            <View style={styles.progressBar}>
              <View style={[styles.progressFill, { 
                width: `${hvacState.filter_health_pct || 100}%`,
                backgroundColor: (hvacState.filter_health_pct || 100) > 50 ? '#4CAF50' : 
                                  (hvacState.filter_health_pct || 100) > 20 ? '#FF9800' : '#F44336'
              }]} />
            </View>
            <Text style={styles.progressText}>
              {Math.round(hvacState.filter_health_pct || 100)}% remaining
            </Text>
          </View>

          {/* Quick Actions */}
          <View style={styles.card}>
            <Text style={styles.cardTitle}>⚡ Quick Actions</Text>
            <View style={styles.actionGrid}>
              <TouchableOpacity 
                style={styles.actionButton}
                onPress={() => sendCommand({ fan_override: true })}
              >
                <Text style={styles.actionIcon}>💨</Text>
                <Text style={styles.actionLabel}>Fan On</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.actionButton}
                onPress={() => sendCommand({ fan_override: false })}
              >
                <Text style={styles.actionIcon}>⏹️</Text>
                <Text style={styles.actionLabel}>Fan Off</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.actionButton}
                onPress={() => sendCommand({ bathroom_exhaust: true })}
              >
                <Text style={styles.actionIcon}>🚿</Text>
                <Text style={styles.actionLabel}>Bath Exhaust</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.actionButton}
                onPress={() => sendCommand({ range_hood: true })}
              >
                <Text style={styles.actionIcon}>🍳</Text>
                <Text style={styles.actionLabel}>Range Hood</Text>
              </TouchableOpacity>
            </View>
          </View>
        </>
      )}
    </ScrollView>
  );
}

// ========== NAVIGATION ==========

const Tab = createBottomTabNavigator();

function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: '#2196F3',
          tabBarInactiveTintColor: '#999',
          tabBarStyle: { paddingBottom: 8, height: 60 },
        }}
      >
        <Tab.Screen 
          name="Home" 
          component={HomeScreen}
          options={{ tabBarLabel: 'Home', tabBarIcon: () => <Text>🏠</Text> }}
        />
        <Tab.Screen 
          name="Exposure" 
          component={ExposureScreen}
          options={{ tabBarLabel: 'Exposure', tabBarIcon: () => <Text>🫁</Text> }}
        />
        <Tab.Screen 
          name="Alerts" 
          component={AlertsScreen}
          options={{ tabBarLabel: 'Alerts', tabBarIcon: () => <Text>🔔</Text> }}
        />
        <Tab.Screen 
          name="HVAC" 
          component={HVACControlScreen}
          options={{ tabBarLabel: 'HVAC', tabBarIcon: () => <Text>⚙️</Text> }}
        />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

// ========== STYLES ==========

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
    padding: 16,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  headerTitle: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#333',
  },
  headerSubtitle: {
    fontSize: 14,
    color: '#666',
  },
  overallCard: {
    backgroundColor: '#fff',
    borderRadius: 16,
    padding: 20,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 8,
    elevation: 4,
  },
  cardTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
    marginBottom: 12,
  },
  overallRow: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  overallInfo: {
    flex: 1,
    marginLeft: 16,
  },
  aqiCategory: {
    fontSize: 22,
    fontWeight: 'bold',
  },
  aqiEmoji: {
    fontSize: 40,
    marginTop: 8,
  },
  aqiNote: {
    fontSize: 14,
    color: '#666',
    marginTop: 4,
  },
  sectionTitle: {
    fontSize: 20,
    fontWeight: '600',
    color: '#333',
    marginTop: 8,
    marginBottom: 12,
  },
  roomCard: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.08,
    shadowRadius: 4,
    elevation: 2,
  },
  roomHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 12,
  },
  aqiBadge: {
    width: 48,
    height: 48,
    borderRadius: 24,
    justifyContent: 'center',
    alignItems: 'center',
  },
  aqiBadgeText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  roomInfo: {
    flex: 1,
    marginLeft: 12,
  },
  roomName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  roomStatus: {
    fontSize: 13,
    color: '#666',
  },
  roomTime: {
    fontSize: 12,
    color: '#999',
  },
  roomMetrics: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  metric: {
    alignItems: 'center',
  },
  metricValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  metricLabel: {
    fontSize: 11,
    color: '#999',
    marginTop: 2,
  },
  moldWarning: {
    backgroundColor: '#FFF3E0',
    borderRadius: 8,
    padding: 8,
    marginTop: 8,
  },
  moldWarningText: {
    fontSize: 13,
    color: '#E65100',
  },
  gaugeContainer: {
    justifyContent: 'center',
    alignItems: 'center',
  },
  gaugeOuter: {
    borderWidth: 8,
    justifyContent: 'center',
    alignItems: 'center',
  },
  gaugeInner: {
    justifyContent: 'center',
    alignItems: 'center',
  },
  gaugeValue: {
    fontSize: 36,
    fontWeight: 'bold',
  },
  gaugeLabel: {
    fontSize: 14,
    color: '#666',
  },
  alertCard: {
    backgroundColor: '#fff',
    borderRadius: 8,
    padding: 12,
    marginBottom: 8,
    borderLeftWidth: 4,
  },
  alertCritical: {
    borderLeftColor: '#7E0023',
    backgroundColor: '#FFEBEE',
  },
  alertDanger: {
    borderLeftColor: '#F44336',
    backgroundColor: '#FFF3E0',
  },
  alertWarning: {
    borderLeftColor: '#FF9800',
    backgroundColor: '#FFF8E1',
  },
  alertAcknowledged: {
    opacity: 0.5,
  },
  alertMessage: {
    fontSize: 14,
    fontWeight: '500',
    color: '#333',
  },
  alertFooter: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: 4,
  },
  alertTime: {
    fontSize: 12,
    color: '#999',
  },
  ackButton: {
    backgroundColor: '#2196F3',
    borderRadius: 4,
    paddingHorizontal: 12,
    paddingVertical: 4,
  },
  ackButtonText: {
    color: '#fff',
    fontSize: 12,
  },
  exposureCard: {
    backgroundColor: '#fff',
    borderRadius: 16,
    padding: 20,
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 8,
    elevation: 4,
  },
  exposureMetrics: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    width: '100%',
    marginTop: 16,
  },
  exposureStats: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    width: '100%',
    marginTop: 16,
    paddingTop: 16,
    borderTopWidth: 1,
    borderTopColor: '#eee',
  },
  statBox: {
    alignItems: 'center',
  },
  statValue: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#333',
  },
  statLabel: {
    fontSize: 12,
    color: '#999',
  },
  activityLabel: {
    fontSize: 14,
    color: '#666',
    marginTop: 12,
  },
  symptomBanner: {
    backgroundColor: '#FFEBEE',
    borderRadius: 8,
    padding: 8,
    marginTop: 12,
  },
  symptomText: {
    fontSize: 13,
    color: '#C62828',
    textAlign: 'center',
  },
  filterRow: {
    flexDirection: 'row',
    marginBottom: 16,
  },
  filterTab: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    backgroundColor: '#eee',
    marginRight: 8,
  },
  filterActive: {
    backgroundColor: '#2196F3',
  },
  filterText: {
    fontSize: 13,
    color: '#666',
  },
  filterTextActive: {
    color: '#fff',
  },
  card: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.08,
    shadowRadius: 4,
    elevation: 2,
  },
  progressBar: {
    height: 12,
    backgroundColor: '#eee',
    borderRadius: 6,
    overflow: 'hidden',
    marginTop: 8,
  },
  progressFill: {
    height: '100%',
    borderRadius: 6,
  },
  progressText: {
    fontSize: 13,
    color: '#666',
    marginTop: 4,
  },
  actionGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
  },
  actionButton: {
    width: '48%',
    backgroundColor: '#E3F2FD',
    borderRadius: 12,
    padding: 16,
    alignItems: 'center',
    marginBottom: 12,
  },
  actionIcon: {
    fontSize: 28,
  },
  actionLabel: {
    fontSize: 13,
    color: '#1565C0',
    marginTop: 4,
  },
});

export default App;