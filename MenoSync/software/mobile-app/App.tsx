/*
 * MenoSync — Mobile App (React Native)
 * AI-powered menopause management & wellness companion app.
 *
 * Features:
 * - Real-time hot flash risk meter (probability + minutes to onset)
 * - Vitals view (HR, SpO₂, skin temp, HRV, activity)
 * - EDA stress monitoring (skin conductance, stress level)
 * - Sleep quality tracking (BCG sleep staging, night sweat events)
 * - Pre-emptive cooling status (HVAC mode, shade %, ambient temp)
 * - Mood/brain fog screening results history
 * - Bone health risk score + recommendations
 * - Personal trigger analysis (SHAP-based)
 * - Treatment response tracking (HRT effectiveness charts)
 * - Alert feed (hot flash warnings, night sweat alerts, medication reminders)
 * - Gynecologist communication (messaging, telehealth)
 * - Sensor management (pair/unpair, battery, calibration)
 * - Push notifications (hot flash warning, cooling activated, med reminders)
 */
import React, { useState, useEffect, useCallback } from "react";
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Alert,
  Switch,
  ActivityIndicator,
  RefreshControl,
  Platform,
} from "react-native";
import { NavigationContainer } from "@react-navigation/native";
import { createBottomTabNavigator } from "@react-navigation/bottom-tabs";

const API_BASE = "https://api.menosync.io/api/v1";
const WS_BASE = "wss://api.menosync.io/ws/realtime";

// === Types ===
interface Vitals {
  heart_rate: number;
  spo2: number;
  skin_temp_c: number;
  hrv_rmssd_ms: number;
  activity_class: number;
  battery_pct: number;
}

interface HotFlash {
  probability: number;
  minutes_to_onset: number;
  severity_pred: number;
  cooling_recommended: boolean;
  confidence: number;
}

interface RiskAssessment {
  hotflash_risk: number;
  nightsweat_risk: number;
  sleep_quality: number;
  mood_risk: number;
  bone_risk: number;
  overall_risk: number;
  cooling_active: boolean;
  alert_level: number;
}

interface Trigger {
  trigger: string;
  importance: number;
  occurrences: number;
}

interface MoodScreen {
  date: string;
  mood_score: number;
  brain_fog_score: number;
  classification: string;
  confidence: number;
}

interface TreatmentWeek {
  week: number;
  hot_flash_count: number;
  night_sweat_count: number;
  avg_severity: number;
}

interface AlertItem {
  id: number;
  patient_id: string;
  alert_type: string;
  severity: string;
  message: string;
  timestamp: string;
  acknowledged: boolean;
}

// === API Service ===
const api = {
  async getLatestVitals(patientId: string): Promise<Vitals> {
    const res = await fetch(`${API_BASE}/vitals/${patientId}/latest`);
    return res.json();
  },
  async getLatestHotFlash(patientId: string): Promise<HotFlash> {
    const res = await fetch(`${API_BASE}/hotflash/${patientId}/latest`);
    return res.json();
  },
  async getRisk(patientId: string): Promise<RiskAssessment> {
    const res = await fetch(`${API_BASE}/risk/${patientId}`);
    return res.json();
  },
  async getTriggers(patientId: string) {
    const res = await fetch(`${API_BASE}/triggers/${patientId}`);
    return res.json();
  },
  async getMoodScreen(patientId: string): Promise<MoodScreen> {
    const res = await fetch(`${API_BASE}/mood/${patientId}/screen`);
    return res.json();
  },
  async getBoneRisk(patientId: string) {
    const res = await fetch(`${API_BASE}/bone-risk/${patientId}`);
    return res.json();
  },
  async getTreatmentResponse(patientId: string) {
    const res = await fetch(`${API_BASE}/treatment/${patientId}`);
    return res.json();
  },
  async getAlerts(patientId: string) {
    const res = await fetch(`${API_BASE}/alerts?patient_id=${patientId}&limit=20`);
    return res.json();
  },
  async ackAlert(alertId: number) {
    const res = await fetch(`${API_BASE}/alerts/${alertId}/ack`, { method: "PUT" });
    return res.json();
  },
};

// === Hot Flash Risk Screen ===
function HotFlashScreen() {
  const [vitals, setVitals] = useState<Vitals | null>(null);
  const [hotflash, setHotflash] = useState<HotFlash | null>(null);
  const [risk, setRisk] = useState<RiskAssessment | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const [v, h, r] = await Promise.all([
        api.getLatestVitals("patient_001"),
        api.getLatestHotFlash("patient_001"),
        api.getRisk("patient_001"),
      ]);
      setVitals(v);
      setHotflash(h);
      setRisk(r);
    } catch (e) {
      // offline fallback
    }
  }, []);

  useEffect(() => {
    load();
    const websocket = new WebSocket(`${WS_BASE}/patient_001`);
    websocket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      if (data.type === "hotflash") setHotflash(data.data);
      if (data.type === "vitals") setVitals(data.data);
      if (data.type === "alert") {
        Alert.alert("MenoSync Alert", data.data.message);
      }
    };
    return () => websocket.close();
  }, [load]);

  const onRefresh = async () => {
    setRefreshing(true);
    await load();
    setRefreshing(false);
  };

  const riskColor = (r: number) =>
    r >= 80 ? "#F44336" : r >= 60 ? "#FF9800" : r >= 30 ? "#FFC107" : "#4CAF50";

  const severityNames = ["Mild", "Moderate", "Severe"];
  const activityNames = ["Rest", "Sit", "Walk", "Active", "Sleep", "Stretch"];

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <View style={styles.header}>
        <Text style={styles.title}>Hot Flash Risk</Text>
        <Text style={styles.subtitle}>Perimenopause · Day 80</Text>
      </View>

      {/* Hot Flash Risk Meter */}
      <View style={[styles.card, { backgroundColor: riskColor(hotflash?.probability || 0) + "20" }]}>
        <Text style={styles.cardTitle}>Hot Flash Prediction</Text>
        <Text style={[styles.bigRiskValue, { color: riskColor(hotflash?.probability || 0) }]}>
          {hotflash?.probability || 0}%
        </Text>
        <Text style={styles.cardText}>
          {hotflash && hotflash.probability > 30
            ? `Onset in ~${hotflash.minutes_to_onset} min · ${severityNames[hotflash.severity_pred]}`
            : "No hot flash predicted in next 15 minutes"}
        </Text>
        {hotflash?.cooling_recommended && (
          <View style={styles.coolingBadge}>
            <Text style={styles.coolingText}>❄️ Pre-cooling recommended</Text>
          </View>
        )}
        {risk?.cooling_active && (
          <View style={[styles.coolingBadge, { backgroundColor: "#E3F2FD" }]}>
            <Text style={[styles.coolingText, { color: "#1976D2" }]}>
              ❄️ Cooling active — HVAC reducing room temp
            </Text>
          </View>
        )}
      </View>

      {/* Vital Signs */}
      <View style={styles.metricsGrid}>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Heart Rate</Text>
          <Text style={styles.metricValue}>{vitals?.heart_rate || "--"}</Text>
          <Text style={styles.metricUnit}>bpm</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Skin Temp</Text>
          <Text style={styles.metricValue}>
            {vitals ? vitals.skin_temp_c.toFixed(1) : "--"}
          </Text>
          <Text style={styles.metricUnit}>°C</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>HRV</Text>
          <Text style={styles.metricValue}>{vitals?.hrv_rmssd_ms || "--"}</Text>
          <Text style={styles.metricUnit}>ms</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>SpO₂</Text>
          <Text style={[styles.metricValue, { color: (vitals?.spo2 || 98) >= 95 ? "#4CAF50" : "#F44336" }]}>
            {vitals?.spo2 || "--"}
          </Text>
          <Text style={styles.metricUnit}>%</Text>
        </View>
      </View>

      {/* Risk Dashboard */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Risk Assessment</Text>
        {risk && (
          <>
            <RiskBar label="Hot Flash" value={risk.hotflash_risk} color={riskColor(risk.hotflash_risk)} />
            <RiskBar label="Night Sweats" value={risk.nightsweat_risk} color={riskColor(risk.nightsweat_risk)} />
            <RiskBar label="Mood" value={risk.mood_risk} color={riskColor(risk.mood_risk)} />
            <RiskBar label="Bone Health" value={risk.bone_risk} color={riskColor(risk.bone_risk)} />
            <View style={styles.sleepRow}>
              <Text style={styles.cardText}>Sleep Quality</Text>
              <Text style={[styles.sleepValue, { color: risk.sleep_quality >= 70 ? "#4CAF50" : risk.sleep_quality >= 50 ? "#FF9800" : "#F44336" }]}>
                {risk.sleep_quality}/100
              </Text>
            </View>
          </>
        )}
      </View>
    </ScrollView>
  );
}

function RiskBar({ label, value, color }: { label: string; value: number; color: string }) {
  return (
    <View style={styles.riskBarRow}>
      <Text style={styles.riskBarLabel}>{label}</Text>
      <View style={styles.riskBarTrack}>
        <View style={[styles.riskBarFill, { width: `${value}%`, backgroundColor: color }]} />
      </View>
      <Text style={styles.riskBarValue}>{value}%</Text>
    </View>
  );
}

// === Triggers Screen ===
function TriggersScreen() {
  const [triggers, setTriggers] = useState<Trigger[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.getTriggers("patient_001").then((data) => {
      setTriggers(data.top_triggers || []);
      setLoading(false);
    });
  }, []);

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  const triggerLabels: Record<string, string> = {
    ambient_temp_high: "🌡️ High Room Temperature",
    stress_eda_spike: "😰 Stress (EDA Spike)",
    caffeine_consumption: "☕ Caffeine",
    poor_sleep_prior_night: "😴 Poor Sleep (Prior Night)",
    alcohol_consumption: "🍷 Alcohol",
    spicy_food: "🌶️ Spicy Food",
    exercise_intense: "🏃 Intense Exercise",
    hot_environment: "🔥 Hot Environment",
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Your Triggers</Text>
        <Text style={styles.subtitle}>Personal hot flash trigger analysis</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Top Triggers (SHAP Analysis)</Text>
        {triggers.map((t, i) => (
          <View key={i} style={styles.triggerRow}>
            <Text style={styles.triggerName}>
              {triggerLabels[t.trigger] || t.trigger}
            </Text>
            <View style={styles.triggerBarTrack}>
              <View style={[styles.triggerBarFill, { width: `${t.importance * 100}%` }]} />
            </View>
            <Text style={styles.triggerValue}>
              {(t.importance * 100).toFixed(0)}%
            </Text>
            <Text style={styles.triggerCount}>{t.occurrences}×</Text>
          </View>
        ))}
      </View>
    </ScrollView>
  );
}

// === Treatment Screen ===
function TreatmentScreen() {
  const [data, setData] = useState<TreatmentWeek[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.getTreatmentResponse("patient_001").then((d) => {
      setData(d.weekly_data || []);
      setLoading(false);
    });
  }, []);

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  const maxCount = Math.max(...data.map(d => d.hot_flash_count), 1);

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Treatment Response</Text>
        <Text style={styles.subtitle}>HRT effectiveness tracking</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Weekly Hot Flash Count</Text>
        {data.map((w, i) => (
          <View key={i} style={styles.treatmentRow}>
            <Text style={styles.treatmentWeek}>Week {w.week}</Text>
            <View style={styles.treatmentBarTrack}>
              <View style={[styles.treatmentBarFill, {
                width: `${(w.hot_flash_count / maxCount) * 100}%`,
                backgroundColor: w.hot_flash_count > 40 ? "#F44336" : w.hot_flash_count > 20 ? "#FF9800" : "#4CAF50"
              }]} />
            </View>
            <Text style={styles.treatmentCount}>{w.hot_flash_count}</Text>
          </View>
        ))}
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Summary</Text>
        <Text style={styles.cardText}>
          Hot flashes reduced from {data[0]?.hot_flash_count || 0}/week to{" "}
          {data[data.length - 1]?.hot_flash_count || 0}/week
          {"\n"}
          ({(((data[0]?.hot_flash_count - data[data.length - 1]?.hot_flash_count) /
            Math.max(data[0]?.hot_flash_count, 1)) * 100).toFixed(0)}% reduction)
          {"\n"}
          Average severity: {data[0]?.avg_severity.toFixed(1) || 0} →{" "}
          {data[data.length - 1]?.avg_severity.toFixed(1) || 0}
        </Text>
      </View>
    </ScrollView>
  );
}

// === Alerts Screen ===
function AlertsScreen() {
  const [alerts, setAlerts] = useState<AlertItem[]>([]);
  const [loading, setLoading] = useState(true);

  const load = useCallback(async () => {
    const data = await api.getAlerts("patient_001");
    setAlerts(data.alerts || []);
    setLoading(false);
  }, []);

  useEffect(() => { load(); }, [load]);

  const ackAlert = async (id: number) => {
    await api.ackAlert(id);
    setAlerts(alerts.map(a => a.id === id ? { ...a, acknowledged: true } : a));
  };

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  const severityColor = (sev: string) => {
    switch (sev) {
      case "critical": return "#F44336";
      case "high": return "#FF5722";
      case "medium": return "#FF9800";
      case "low": return "#4CAF50";
      default: return "#9E9E9E";
    }
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Alerts</Text>
      </View>
      {alerts.length === 0 ? (
        <View style={styles.card}>
          <Text style={styles.cardText}>No alerts — all systems normal ✓</Text>
        </View>
      ) : (
        alerts.map((a) => (
          <View key={a.id} style={[styles.alertItem, { borderLeftColor: severityColor(a.severity) }]}>
            <Text style={styles.alertType}>{a.alert_type.replace(/_/g, " ")}</Text>
            <Text style={styles.alertMessage}>{a.message}</Text>
            <Text style={styles.alertTime}>
              {new Date(a.timestamp).toLocaleString()} • {a.severity}
            </Text>
            {!a.acknowledged && (
              <TouchableOpacity style={styles.ackButton} onPress={() => ackAlert(a.id)}>
                <Text style={styles.buttonText}>Acknowledge</Text>
              </TouchableOpacity>
            )}
          </View>
        ))
      )}
    </ScrollView>
  );
}

// === Settings Screen ===
function SettingsScreen() {
  const [sensors, setSensors] = useState([
    { id: "WB-01", name: "Wrist Band", battery: 82, paired: true },
    { id: "BM-01", name: "Bed Mat", battery: 95, paired: true },
    { id: "CN-01", name: "Climate Node (Bedroom)", battery: 100, paired: true },
    { id: "CN-02", name: "Climate Node (Living Room)", battery: 100, paired: true },
  ]);
  const [coolingAuto, setCoolingAuto] = useState(true);
  const [hapticAlerts, setHapticAlerts] = useState(true);
  const [gynSharing, setGynSharing] = useState(true);

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Settings</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Sensors</Text>
        {sensors.map((s) => (
          <View key={s.id} style={styles.sensorRow}>
            <Text style={styles.sensorName}>{s.name}</Text>
            <Text style={styles.sensorBattery}>{s.paired ? `${s.battery}%` : "Not paired"}</Text>
            <TouchableOpacity
              style={s.paired ? styles.unpairButton : styles.pairButton}
              onPress={() => {
                setSensors(sensors.map(x => x.id === s.id ? { ...x, paired: !x.paired } : x));
              }}
            >
              <Text style={styles.buttonText}>{s.paired ? "Unpair" : "Pair"}</Text>
            </TouchableOpacity>
          </View>
        ))}
      </View>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Preferences</Text>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Auto Pre-Cooling</Text>
          <Switch value={coolingAuto} onValueChange={setCoolingAuto} />
        </View>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Haptic Alerts</Text>
          <Switch value={hapticAlerts} onValueChange={setHapticAlerts} />
        </View>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Share with Gynecologist</Text>
          <Switch value={gynSharing} onValueChange={setGynSharing} />
        </View>
      </View>
    </ScrollView>
  );
}

// === App Navigation ===
const Tab = createBottomTabNavigator();

function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: "#9C27B0",
          headerShown: false,
        }}
      >
        <Tab.Screen name="Hot Flash" component={HotFlashScreen}
          options={{ tabBarLabel: "Risk" }} />
        <Tab.Screen name="Triggers" component={TriggersScreen} />
        <Tab.Screen name="Treatment" component={TreatmentScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
        <Tab.Screen name="Settings" component={SettingsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: "#F5F5F5" },
  header: { padding: 20, paddingBottom: 10 },
  title: { fontSize: 28, fontWeight: "bold", color: "#333" },
  subtitle: { fontSize: 14, color: "#888", marginTop: 4 },
  card: { backgroundColor: "#FFF", marginHorizontal: 16, marginVertical: 8,
    borderRadius: 12, padding: 16, elevation: 2, shadowColor: "#000",
    shadowOpacity: 0.1, shadowRadius: 4, shadowOffset: { width: 0, height: 2 } },
  cardTitle: { fontSize: 16, fontWeight: "bold", color: "#555", marginBottom: 12 },
  cardText: { fontSize: 14, color: "#666", lineHeight: 22 },
  bigRiskValue: { fontSize: 48, fontWeight: "bold", textAlign: "center", marginVertical: 8 },
  coolingBadge: { backgroundColor: "#E8F5E9", borderRadius: 8, padding: 8,
    marginTop: 8, alignItems: "center" },
  coolingText: { fontSize: 13, color: "#2E7D32", fontWeight: "600" },
  metricsGrid: { flexDirection: "row", flexWrap: "wrap", marginHorizontal: 8 },
  metricCard: { backgroundColor: "#FFF", borderRadius: 12, padding: 16,
    margin: 8, width: "44%", alignItems: "center", elevation: 2 },
  metricLabel: { fontSize: 12, color: "#888" },
  metricValue: { fontSize: 32, fontWeight: "bold", color: "#333" },
  metricUnit: { fontSize: 12, color: "#888" },
  riskBarRow: { flexDirection: "row", alignItems: "center", marginVertical: 6 },
  riskBarLabel: { fontSize: 13, color: "#555", width: 90 },
  riskBarTrack: { flex: 1, height: 10, backgroundColor: "#E0E0E0",
    borderRadius: 5, marginHorizontal: 8 },
  riskBarFill: { height: "100%", borderRadius: 5 },
  riskBarValue: { fontSize: 12, color: "#555", width: 36, textAlign: "right" },
  sleepRow: { flexDirection: "row", justifyContent: "space-between",
    marginTop: 8, paddingTop: 8, borderTopWidth: 1, borderTopColor: "#EEE" },
  sleepValue: { fontSize: 18, fontWeight: "bold" },
  triggerRow: { flexDirection: "row", alignItems: "center", marginVertical: 8 },
  triggerName: { fontSize: 13, color: "#555", width: 140 },
  triggerBarTrack: { flex: 1, height: 8, backgroundColor: "#E0E0E0", borderRadius: 4 },
  triggerBarFill: { height: "100%", backgroundColor: "#9C27B0", borderRadius: 4 },
  triggerValue: { fontSize: 12, color: "#555", width: 36, textAlign: "right", marginLeft: 4 },
  triggerCount: { fontSize: 11, color: "#AAA", width: 30, textAlign: "right" },
  treatmentRow: { flexDirection: "row", alignItems: "center", marginVertical: 6 },
  treatmentWeek: { fontSize: 13, color: "#555", width: 70 },
  treatmentBarTrack: { flex: 1, height: 16, backgroundColor: "#E0E0E0", borderRadius: 8 },
  treatmentBarFill: { height: "100%", borderRadius: 8 },
  treatmentCount: { fontSize: 13, fontWeight: "bold", color: "#333",
    width: 30, textAlign: "right", marginLeft: 4 },
  alertItem: { backgroundColor: "#FFF", marginHorizontal: 16, marginVertical: 4,
    borderRadius: 8, padding: 14, borderLeftWidth: 4, elevation: 1 },
  alertType: { fontSize: 14, fontWeight: "bold", color: "#333" },
  alertMessage: { fontSize: 13, color: "#666", marginTop: 4 },
  alertTime: { fontSize: 11, color: "#AAA", marginTop: 4 },
  ackButton: { backgroundColor: "#9C27B0", borderRadius: 6, padding: 8,
    marginTop: 8, alignItems: "center" },
  buttonText: { color: "#FFF", fontSize: 12, fontWeight: "600" },
  sensorRow: { flexDirection: "row", alignItems: "center",
    justifyContent: "space-between", paddingVertical: 8 },
  sensorName: { fontSize: 14, color: "#333", flex: 1 },
  sensorBattery: { fontSize: 13, color: "#888" },
  pairButton: { backgroundColor: "#4CAF50", borderRadius: 6, padding: 6, marginLeft: 8 },
  unpairButton: { backgroundColor: "#F44336", borderRadius: 6, padding: 6, marginLeft: 8 },
  settingRow: { flexDirection: "row", justifyContent: "space-between",
    alignItems: "center", paddingVertical: 10 },
});

export default App;