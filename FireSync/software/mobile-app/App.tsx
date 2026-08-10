// FireSync — React Native Mobile App
//
// AI-powered home fire prevention, detection, suppression & escape guidance app.
// Screens: Dashboard, Fire Events, Sentinels, Stove, Panel, Escape, Occupants,
// Risk Forecast, Alerts, Rooms, Emergency, Settings.
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

const FLAME_CLASSES = {
  0: 'Normal', 1: 'Cooking', 2: 'Steam', 3: 'Cigarette',
  4: 'Candle', 5: 'Smoldering', 6: 'Flaming Fire',
};

const ARC_CLASSES = {
  0: 'Normal', 1: 'Series Arc', 2: 'Parallel Arc', 3: 'Overload',
};

const PANTEMP_CLASSES = {
  0: 'Safe Cooking', 1: 'Overheating', 2: 'Oil Smoking', 3: 'Flaming',
};

const RISK_COLORS = {
  low: '#4caf50', moderate: '#ff9800', high: '#f44336', very_high: '#9c27b0',
};

// ─── Dashboard Screen ──────────────────────────────────────────────────────

function DashboardScreen() {
  const [devices, setDevices] = useState([]);
  const [activeFire, setActiveFire] = useState({ active: false });
  const [riskForecast, setRiskForecast] = useState(null);
  const [occupants, setOccupants] = useState({});
  const [alerts, setAlerts] = useState([]);
  const [suppression, setSuppression] = useState(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAll = useCallback(async () => {
    try {
      const [devs, fire, risk, occ, alts, supp] = await Promise.all([
        fetch(`${API_BASE}/devices`).then(r => r.json()),
        fetch(`${API_BASE}/fire/active`).then(r => r.json()),
        fetch(`${API_BASE}/risk/forecast`).then(r => r.json()),
        fetch(`${API_BASE}/occupants`).then(r => r.json()),
        fetch(`${API_BASE}/alerts?limit=5`).then(r => r.json()),
        fetch(`${API_BASE}/suppression/status`).then(r => r.json()),
      ]);
      setDevices(devs);
      setActiveFire(fire);
      setRiskForecast(risk);
      setOccupants(occ);
      setAlerts(alts);
      setSuppression(supp);
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
        <Text style={styles.h1}>FireSync</Text>
        <Text style={styles.subtitle}>Home Fire Safety System</Text>

        {/* Active Fire Alert */}
        {activeFire.active && (
          <View style={styles.fireAlertCard}>
            <Text style={styles.fireAlertTitle}>🔥 FIRE DETECTED</Text>
            <Text style={styles.fireAlertText}>Room: {activeFire.room_id}</Text>
            <Text style={styles.fireAlertText}>Exit: {activeFire.safe_exit}</Text>
            <TouchableOpacity style={styles.silenceButton} onPress={() => {
              fetch(`${API_BASE}/alarm/silence`, { method: 'POST' });
              Alert.alert('Alarm Silenced', 'Fire alarm silenced');
            }}>
              <Text style={styles.silenceText}>Silence Alarm (False Alarm)</Text>
            </TouchableOpacity>
          </View>
        )}

        {/* Fire Risk Score */}
        {riskForecast && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Fire Risk (7-day forecast)</Text>
            <Text style={[styles.riskScore, { color: RISK_COLORS[riskForecast.level] || '#fff' }]}>
              {riskForecast.score}/100 — {riskForecast.level.toUpperCase()}
            </Text>
            <Text style={styles.cardRow}>{riskForecast.recommendation}</Text>
          </View>
        )}

        {/* Device Status */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Device Status</Text>
          {devices.map((d, i) => (
            <Text key={i} style={styles.cardRow}>
              {d.device_type === 'hub' ? '🏠' :
               d.device_type === 'sentinel' ? '🛡️' :
               d.device_type === 'stove' ? '🍳' :
               d.device_type === 'panel' ? '⚡' :
               d.device_type === 'escape' ? '🚪' : '📡'}
              {' '}{d.name}: {d.online ? '✅ Online' : '❌ Offline'}
            </Text>
          ))}
        </View>

        {/* Suppression Status */}
        {suppression && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Suppression Systems</Text>
            <Text style={styles.cardRow}>
              🍳 Stove Gas Valve: {suppression.stove_valve === 'open' ? '✅ Open' : '🛑 Closed'}
            </Text>
            <Text style={styles.cardRow}>
              ⚡ Panel Breaker: {suppression.panel_shunt_tripped ? '🛑 Tripped' : '✅ Normal'}
            </Text>
            <Text style={styles.cardRow}>
              💨 HVAC: {suppression.hvac_off ? '🛑 Off' : '✅ Running'}
            </Text>
          </View>
        )}

        {/* Occupant Map */}
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Occupants</Text>
          {Object.keys(occupants).length === 0 ? (
            <Text style={styles.empty}>No occupancy data</Text>
          ) : (
            Object.entries(occupants).map(([room, status], i) => (
              <Text key={i} style={styles.cardRow}>
                {status === 'occupied' ? '👤' : '🚪'} {room}: {status}
              </Text>
            ))
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
          fetch(`${API_BASE}/alarm/test`, { method: 'POST' });
          Alert.alert('Test Alarm', 'Monthly test alarm triggered');
        }}>
          <Text style={styles.actionText}>🔔 Test Alarm</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.silenceButton} onPress={() => {
          fetch(`${API_BASE}/alarm/silence`, { method: 'POST' });
          Alert.alert('Silenced', 'Alarm silenced');
        }}>
          <Text style={styles.silenceText}>🔇 Silence Alarm</Text>
        </TouchableOpacity>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Fire Events Screen ──────────────────────────────────────────────────────

function FireEventsScreen() {
  const [events, setEvents] = useState([]);
  const [activeFire, setActiveFire] = useState({ active: false });

  useEffect(() => {
    fetch(`${API_BASE}/fire/events`).then(r => r.json()).then(setEvents).catch(console.log);
    fetch(`${API_BASE}/fire/active`).then(r => r.json()).then(setActiveFire).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Fire Events</Text>
        <Text style={styles.subtitle}>Detection history & active alerts</Text>

        {activeFire.active && (
          <View style={styles.fireAlertCard}>
            <Text style={styles.fireAlertTitle}>🔥 ACTIVE FIRE</Text>
            <Text style={styles.fireAlertText}>Room: {activeFire.room_id}</Text>
            <Text style={styles.fireAlertText}>Exit: {activeFire.safe_exit}</Text>
            <TouchableOpacity style={styles.silenceButton} onPress={() => {
              fetch(`${API_BASE}/fire/${activeFire.id || 0}/false`, { method: 'POST' });
            }}>
              <Text style={styles.silenceText}>Mark as False Alarm</Text>
            </TouchableOpacity>
          </View>
        )}

        {events.length === 0 ? (
          <Text style={styles.empty}>No fire events recorded</Text>
        ) : (
          events.map((e, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>
                {e.resolved ? '✅' : '🔥'} {e.fire_class} — Room {e.room_id}
                {e.false_alarm && ' (False Alarm)'}
              </Text>
              <Text style={styles.cardRow}>Confidence: {e.confidence}%</Text>
              <Text style={styles.cardRow}>Smoke: {e.smoke_pm25} μg/m³</Text>
              <Text style={styles.cardRow}>CO: {e.co_ppm} ppm</Text>
              <Text style={styles.cardRow}>Temp: {e.temp_c?.toFixed?.(1)}°C</Text>
              <Text style={styles.cardRow}>Actions: {e.actions_taken?.join(', ')}</Text>
              <Text style={styles.cardRow}>911 Dispatched: {e.dispatch_911 ? 'Yes' : 'No'}</Text>
              {!e.acknowledged && (
                <TouchableOpacity style={styles.ackButton} onPress={() => {
                  fetch(`${API_BASE}/fire/${e.id}/ack`, { method: 'POST' });
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

// ─── Sentinels Screen ──────────────────────────────────────────────────────

function SentinelsScreen() {
  const [sentinels, setSentinels] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/sentinels`).then(r => r.json()).then(setSentinels).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Room Sentinels</Text>
        <Text style={styles.subtitle}>Multi-sensor fire detection nodes</Text>
        {sentinels.map((s, i) => (
          <View key={i} style={styles.card}>
            <Text style={styles.cardTitle}>🛡️ {s.room_name}</Text>
            <Text style={styles.cardRow}>Node ID: {s.node_id}</Text>
            <Text style={styles.cardRow}>Online: {s.online ? '✅' : '❌'}</Text>
            <Text style={styles.cardRow}>Battery: {(s.battery_v).toFixed(2)}V</Text>
            <TouchableOpacity onPress={() => {
              fetch(`${API_BASE}/sentinels/${s.node_id}`).then(r => r.json()).then(detail => {
                const t = detail.latest_telemetry || {};
                const msg = [
                  'Smoke: ' + (t.smoke_pm25 ?? 'N/A') + ' ug/m3',
                  'CO: ' + (t.co_ppm ?? 'N/A') + ' ppm',
                  'Temp: ' + (t.temp_c ?? 'N/A') + ' C',
                  'FlameNet: ' + (FLAME_CLASSES[t.flame_class ?? 0] ?? 'N/A'),
                  'Confidence: ' + (t.flame_confidence ?? 0) + '%',
                ].join('\n');
                Alert.alert(s.room_name, msg);
              });
            }}>
              <Text style={styles.actionText}>View Details</Text>
            </TouchableOpacity>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Stove Guard Screen ──────────────────────────────────────────────────────

function StoveScreen() {
  const [telemetry, setTelemetry] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/stove`).then(r => r.json()).then(setTelemetry).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Stove Guard</Text>
        <Text style={styles.subtitle}>Automatic stovetop monitoring & shutoff</Text>
        {telemetry.length === 0 ? (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Stove Status</Text>
            <Text style={styles.cardRow}>No telemetry yet</Text>
            <Text style={styles.cardRow}>Gas Valve: Open (normal)</Text>
            <Text style={styles.cardRow}>Auto-shutoff: 30 min unattended</Text>
            <Text style={styles.cardRow}>Thermal array: MLX90640 monitoring</Text>
            <Text style={styles.cardRow}>Knob sensors: AS5600 ×4 burners</Text>
          </View>
        ) : (
          telemetry.map((t, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>🍳 Stove Guard</Text>
              <Text style={styles.cardRow}>Pan Temp: {t.thermal_max_c?.toFixed?.(1)}°C</Text>
              <Text style={styles.cardRow}>Status: {PANTEMP_CLASSES[t.pantemp_class]}</Text>
              <Text style={styles.cardRow}>Valve: {t.valve_state}</Text>
              <Text style={styles.cardRow}>Timer: {Math.floor(t.timer_remaining_s / 60)}:{(t.timer_remaining_s % 60).toString().padStart(2, '0')}</Text>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Panel Monitor Screen ──────────────────────────────────────────────────

function PanelScreen() {
  const [telemetry, setTelemetry] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/panel`).then(r => r.json()).then(setTelemetry).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Panel Monitor</Text>
        <Text style={styles.subtitle}>Electrical fire prevention</Text>
        {telemetry.length === 0 ? (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>⚡ Electrical Panel</Text>
            <Text style={styles.cardRow}>ArcDetect: Monitoring (8 kHz sampling)</Text>
            <Text style={styles.cardRow}>Bus bar temp: Normal</Text>
            <Text style={styles.cardRow}>Breaker temp: Normal</Text>
            <Text style={styles.cardRow}>Shunt trip: Armed</Text>
          </View>
        ) : (
          telemetry.map((t, i) => (
            <View key={i} style={styles.card}>
              <Text style={styles.cardTitle}>⚡ Panel Monitor</Text>
              <Text style={styles.cardRow}>Current: {t.main_current_a?.toFixed?.(1)}A</Text>
              <Text style={styles.cardRow}>Voltage: {t.voltage_v?.toFixed?.(1)}V</Text>
              <Text style={styles.cardRow}>Power: {t.power_w?.toFixed?.(0)}W</Text>
              <Text style={styles.cardRow}>Bus bar: {t.bus_bar_temp_c}°C</Text>
              <Text style={styles.cardRow}>Arc detect: {ARC_CLASSES[t.arc_fault_class]}</Text>
              <Text style={styles.cardRow}>Shunt: {t.shunt_tripped ? '🛑 Tripped' : '✅ Normal'}</Text>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Escape Screen ──────────────────────────────────────────────────────────

function EscapeScreen() {
  const [route, setRoute] = useState({ active: false });

  useEffect(() => {
    fetch(`${API_BASE}/route`).then(r => r.json()).then(setRoute).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Escape</Text>
        <Text style={styles.subtitle}>Dynamic exit guidance</Text>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>🚪 Escape Controller</Text>
          {route.active ? (
            <>
              <Text style={styles.cardRow}>Route: ACTIVE</Text>
              <Text style={styles.cardRow}>Fire room: {route.fire_room}</Text>
              <Text style={styles.cardRow}>Safe exit: {route.safe_exit}</Text>
              <Text style={styles.cardRow}>Avoid: {route.avoid_rooms?.join(', ')}</Text>
              <Text style={styles.cardRow}>LEDs: Green path illuminated</Text>
              <Text style={styles.cardRow}>Voice: Guidance active</Text>
              <Text style={styles.cardRow}>Doors: Released for escape</Text>
            </>
          ) : (
            <Text style={styles.cardRow}>No active escape route</Text>
          )}
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>How It Works</Text>
          <Text style={styles.cardRow}>• Green LED strips illuminate safe exit path</Text>
          <Text style={styles.cardRow}>• Red LEDs mark fire zone (do not enter)</Text>
          <Text style={styles.cardRow}>• Voice guidance in 8 languages</Text>
          <Text style={styles.cardRow}>• Door locks release automatically</Text>
          <Text style={styles.cardRow}>• Route updates if fire spreads</Text>
          <Text style={styles.cardRow}>• Battery-backed (LiFePO4, 48+ hours)</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Risk Forecast Screen ────────────────────────────────────────────────────

function RiskScreen() {
  const [forecast, setForecast] = useState(null);
  const [weekly, setWeekly] = useState(null);

  useEffect(() => {
    fetch(`${API_BASE}/risk/forecast`).then(r => r.json()).then(setForecast).catch(console.log);
    fetch(`${API_BASE}/risk/weekly`).then(r => r.json()).then(setWeekly).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Fire Risk Forecast</Text>
        <Text style={styles.subtitle}>7-day AI prediction</Text>

        {forecast && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Current Risk Score</Text>
            <Text style={[styles.riskScore, { color: RISK_COLORS[forecast.level] || '#fff' }]}>
              {forecast.score}/100 — {forecast.level.toUpperCase()}
            </Text>
            <Text style={styles.cardRow}>{forecast.recommendation}</Text>
          </View>
        )}

        {forecast && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Risk Factors (SHAP)</Text>
            {forecast.factors?.map((f, i) => (
              <Text key={i} style={styles.cardRow}>
                {f.contribution > 0 ? '↑' : '↓'} {f.feature}: {f.value}
                {'  '}({f.contribution > 0 ? '+' : ''}{f.contribution})
              </Text>
            ))}
          </View>
        )}

        {weekly && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Weekly Report</Text>
            <Text style={styles.cardRow}>Week: {weekly.week}</Text>
            <Text style={styles.cardRow}>Avg risk: {weekly.avg_risk_score}/100</Text>
            <Text style={styles.cardRow}>Peak risk: {weekly.peak_risk_score}/100 ({weekly.peak_day})</Text>
            <Text style={styles.cardRow}>Trend: {weekly.trend}</Text>
            <Text style={styles.cardRow}>Top factors: {weekly.top_factors?.join(', ')}</Text>
          </View>
        )}
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
  const [dispatchStatus, setDispatchStatus] = useState({ active: false });

  useEffect(() => {
    fetch(`${API_BASE}/dispatch/status`).then(r => r.json()).then(setDispatchStatus).catch(console.log);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView>
        <Text style={styles.h1}>Emergency</Text>
        <Text style={styles.subtitle}>911 dispatch & safety</Text>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>911 Dispatch Status</Text>
          <Text style={styles.cardRow}>
            Status: {dispatchStatus.active ? '🚨 DISPATCHED' : '✅ Standby'}
          </Text>
          {dispatchStatus.active && (
            <TouchableOpacity style={styles.cancelButton} onPress={() => {
              fetch(`${API_BASE}/dispatch/cancel`, { method: 'POST' });
              Alert.alert('Dispatch Cancelled', '911 dispatch cancelled (false alarm)');
            }}>
              <Text style={styles.silenceText}>Cancel 911 Dispatch</Text>
            </TouchableOpacity>
          )}
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>How FireSync Responds</Text>
          <Text style={styles.cardRow}>1. FlameNet confirms fire (<200 ms)</Text>
          <Text style={styles.cardRow}>2. Multi-node consensus check</Text>
          <Text style={styles.cardRow}>3. All alarms sound (85-105 dB + strobe)</Text>
          <Text style={styles.cardRow}>4. Escape route computed (Dijkstra)</Text>
          <Text style={styles.cardRow}>5. LED path illuminated (green)</Text>
          <Text style={styles.cardRow}>6. Voice guidance plays</Text>
          <Text style={styles.cardRow}>7. Door locks released</Text>
          <Text style={styles.cardRow}>8. Stove gas valve closed</Text>
          <Text style={styles.cardRow}>9. HVAC shutoff (smoke containment)</Text>
          <Text style={styles.cardRow}>10. 911 dispatched via 4G LTE</Text>
          <Text style={styles.cardRow}>11. You get 60s to cancel (false alarm)</Text>
        </View>

        <TouchableOpacity style={styles.actionButton} onPress={() => {
          fetch(`${API_BASE}/alarm/test`, { method: 'POST' });
          Alert.alert('Test Alarm', 'Monthly test alarm triggered');
        }}>
          <Text style={styles.actionText}>🔔 Monthly Test Alarm</Text>
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
          <Text style={styles.cardRow}>Manage connected FireSync nodes</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Alarms</Text>
          <Text style={styles.cardRow}>Buzzer volume: High (105 dB)</Text>
          <Text style={styles.cardRow}>Strobe: Enabled</Text>
          <Text style={styles.cardRow}>Monthly test: Scheduled (1st of month)</Text>
          <Text style={styles.cardRow}>Fire drill: Quarterly</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Escape</Text>
          <Text style={styles.cardRow}>LED brightness: 80%</Text>
          <Text style={styles.cardRow}>Voice language: English</Text>
          <Text style={styles.cardRow}>Available: EN, ES, ZH, FR, DE, JA, KO, PT</Text>
          <Text style={styles.cardRow}>Auto door release: Enabled</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Suppression</Text>
          <Text style={styles.cardRow}>Stove auto-shutoff: 30 min</Text>
          <Text style={styles.cardRow}>Panel shunt trip: Enabled</Text>
          <Text style={styles.cardRow}>HVAC shutoff: Enabled</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Notifications</Text>
          <Text style={styles.cardRow}>Fire detected: Push + SMS</Text>
          <Text style={styles.cardRow}>CO alarm: Push + SMS</Text>
          <Text style={styles.cardRow}>Arc fault: Push</Text>
          <Text style={styles.cardRow}>Low battery: Push</Text>
          <Text style={styles.cardRow}>Monthly test reminder: Push</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Privacy</Text>
          <Text style={styles.cardRow}>All processing on-device + cloud</Text>
          <Text style={styles.cardRow}>No third-party data sharing</Text>
          <Text style={styles.cardRow}>Thermal images: on-device only</Text>
          <Text style={styles.cardRow}>Data shared only on fire event</Text>
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
        <Tab.Screen name="Fire Events" component={FireEventsScreen} />
        <Tab.Screen name="Sentinels" component={SentinelsScreen} />
        <Tab.Screen name="Stove" component={StoveScreen} />
        <Tab.Screen name="Panel" component={PanelScreen} />
        <Tab.Screen name="Escape" component={EscapeScreen} />
        <Tab.Screen name="Risk" component={RiskScreen} />
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
  cardTitle: { fontSize: 16, fontWeight: 'bold', color: '#ff6b6b', marginBottom: 8 },
  cardRow: { fontSize: 14, color: '#cfd8dc', marginBottom: 4 },
  empty: { fontSize: 14, color: '#8899aa', padding: 16, textAlign: 'center' },
  riskScore: { fontSize: 24, fontWeight: 'bold', marginBottom: 8 },
  fireAlertCard: {
    backgroundColor: '#3a1a1a', borderRadius: 12, padding: 16,
    marginHorizontal: 16, marginBottom: 12, borderWidth: 2, borderColor: '#f44336',
  },
  fireAlertTitle: { fontSize: 20, fontWeight: 'bold', color: '#ff5252', marginBottom: 8 },
  fireAlertText: { fontSize: 14, color: '#ffcdd2', marginBottom: 4 },
  actionButton: {
    backgroundColor: '#1a3a5a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  actionText: { color: '#4fc3f7', fontSize: 14, fontWeight: 'bold' },
  silenceButton: {
    backgroundColor: '#3a2a1a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  silenceText: { color: '#ff9800', fontSize: 14, fontWeight: 'bold' },
  cancelButton: {
    backgroundColor: '#2a1a1a', borderRadius: 8, padding: 14,
    marginHorizontal: 16, marginBottom: 12, alignItems: 'center',
  },
  ackButton: {
    backgroundColor: '#1a3a1a', borderRadius: 6, padding: 10,
    marginTop: 8, alignItems: 'center',
  },
});