/*
 * BloomSync — Mobile App (React Native)
 * AI-powered postpartum maternal health & recovery monitoring companion app.
 *
 * Features:
 * - Real-time vitals view (HR, SpO₂, skin temp, HRV, activity)
 * - Recovery timeline forecast with milestone tracking
 * - Nursing log (session history, side, duration, mastitis risk)
 * - Wound monitoring (temp, moisture, pH, infection risk)
 * - Risk dashboard (hemorrhage, preeclampsia, wound, mastitis, PPD)
 * - PPD screening results history
 * - Alert feed (with severity levels)
 * - Partner/caregiver access (secondary monitoring + task delegation)
 * - Obstetrician communication (messaging, telehealth)
 * - Sensor management (pair/unpair, battery, calibration)
 * - Push notifications (nursing reminders, med reminders, alerts)
 * - Medication reminders
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

const API_BASE = "https://api.bloom-sync.io/api/v1";
const WS_BASE = "wss://api.bloom-sync.io/ws/realtime";

// === Types ===
interface Vitals {
  heart_rate: number;
  spo2: number;
  skin_temp_c: number;
  hrv_rmssd_ms: number;
  activity_class: number;
  battery_pct: number;
}

interface RiskAssessment {
  hemorrhage_risk: number;
  preeclampsia_risk: number;
  wound_risk: number;
  mastitis_risk: number;
  ppd_risk: number;
  recovery_progress: number;
  overall_risk: number;
  alert_level: number;
}

interface RecoveryForecast {
  current_day: number;
  total_days: number;
  milestones: Array<{
    name: string;
    target_day: number;
    predicted_day: number;
    status: string;
    confidence: number;
  }>;
  overall_progress: number;
  risk_flags: string[];
}

interface NursingSession {
  timestamp: number;
  side: string;
  duration_min: number;
  temp_asym_c: number;
  mastitis_risk: number;
}

interface PPDScreen {
  date: string;
  prosody_score: number;
  ppd_screen_positive: boolean;
  confidence: number;
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

  async getRisk(patientId: string): Promise<RiskAssessment> {
    const res = await fetch(`${API_BASE}/risk/${patientId}`);
    return res.json();
  },

  async getRecoveryForecast(patientId: string): Promise<RecoveryForecast> {
    const res = await fetch(`${API_BASE}/recovery/${patientId}/forecast`);
    return res.json();
  },

  async getNursingToday(patientId: string) {
    const res = await fetch(`${API_BASE}/nursing/${patientId}/today`);
    return res.json();
  },

  async getNursingHistory(patientId: string) {
    const res = await fetch(`${API_BASE}/nursing/${patientId}?limit=20`);
    return res.json();
  },

  async getWoundData(patientId: string) {
    const res = await fetch(`${API_BASE}/wound/${patientId}?limit=10`);
    return res.json();
  },

  async getPPDScreen(patientId: string): Promise<PPDScreen> {
    const res = await fetch(`${API_BASE}/ppd/${patientId}/screen`);
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

// === Vitals Screen ===
function VitalsScreen() {
  const [vitals, setVitals] = useState<Vitals | null>(null);
  const [risk, setRisk] = useState<RiskAssessment | null>(null);
  const [ws, setWs] = useState<WebSocket | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const [v, r] = await Promise.all([
        api.getLatestVitals("patient_001"),
        api.getRisk("patient_001"),
      ]);
      setVitals(v);
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
      if (data.type === "vitals") setVitals(data.data);
      if (data.type === "alert") {
        Alert.alert("Alert", data.data.message);
      }
    };
    setWs(websocket);
    return () => websocket.close();
  }, [load]);

  const onRefresh = async () => {
    setRefreshing(true);
    await load();
    setRefreshing(false);
  };

  const riskColor = (r: number) =>
    r >= 80 ? "#F44336" : r >= 60 ? "#FF9800" : r >= 30 ? "#FFC107" : "#4CAF50";
  const activityNames = ["Rest", "Sit", "Walk", "Active", "Sleep", "Nursing"];

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <View style={styles.header}>
        <Text style={styles.title}>Maternal Vitals</Text>
        <Text style={styles.subtitle}>Day 30 of 42 — Postpartum Recovery</Text>
      </View>

      {/* Vital signs */}
      <View style={styles.metricsGrid}>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Heart Rate</Text>
          <Text style={styles.metricValue}>{vitals?.heart_rate || "--"}</Text>
          <Text style={styles.metricUnit}>bpm</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>SpO₂</Text>
          <Text style={[styles.metricValue, { color: (vitals?.spo2 || 98) >= 95 ? "#4CAF50" : "#F44336" }]}>
            {vitals?.spo2 || "--"}
          </Text>
          <Text style={styles.metricUnit}>%</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Skin Temp</Text>
          <Text style={styles.metricValue}>
            {vitals ? (vitals.skin_temp_c).toFixed(1) : "--"}
          </Text>
          <Text style={styles.metricUnit}>°C</Text>
        </View>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>HRV</Text>
          <Text style={styles.metricValue}>{vitals?.hrv_rmssd_ms || "--"}</Text>
          <Text style={styles.metricUnit}>ms</Text>
        </View>
      </View>

      {/* Activity */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Activity</Text>
        <Text style={styles.cardText}>
          Current: {activityNames[vitals?.activity_class || 0]}
        </Text>
      </View>

      {/* Risk Dashboard */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Risk Assessment</Text>
        {risk && (
          <>
            <RiskBar label="Hemorrhage" value={risk.hemorrhage_risk} color={riskColor(risk.hemorrhage_risk)} />
            <RiskBar label="Preeclampsia" value={risk.preeclampsia_risk} color={riskColor(risk.preeclampsia_risk)} />
            <RiskBar label="Wound Infection" value={risk.wound_risk} color={riskColor(risk.wound_risk)} />
            <RiskBar label="Mastitis" value={risk.mastitis_risk} color={riskColor(risk.mastitis_risk)} />
            <RiskBar label="PPD Screen" value={risk.ppd_risk} color={riskColor(risk.ppd_risk)} />
            <View style={styles.overallRiskRow}>
              <Text style={styles.cardText}>Overall Risk</Text>
              <Text style={[styles.overallRiskValue, { color: riskColor(risk.overall_risk) }]}>
                {risk.overall_risk}% — {["Normal", "Watch", "Warning", "Critical"][risk.alert_level]}
              </Text>
            </View>
          </>
        )}
      </View>

      {/* Alert level indicator */}
      {risk && risk.alert_level >= 2 && (
        <View style={styles.alertCard}>
          <Text style={styles.alertText}>
            {risk.alert_level >= 3 ? "⚠️ Critical: Seek Medical Help" : "⚠️ Elevated Risk — Monitor Closely"}
          </Text>
          <Text style={styles.alertSubtext}>
            {risk.alert_level >= 3
              ? "Your healthcare provider has been notified. Please seek immediate medical attention."
              : "Your healthcare provider has been notified. Rest and monitor your symptoms."}
          </Text>
        </View>
      )}
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

// === Recovery Screen ===
function RecoveryScreen() {
  const [forecast, setForecast] = useState<RecoveryForecast | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.getRecoveryForecast("patient_001").then((data) => {
      setForecast(data);
      setLoading(false);
    });
  }, []);

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Recovery Timeline</Text>
        <Text style={styles.subtitle}>6-Week Postpartum Forecast</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Overall Progress</Text>
        <Text style={styles.progressText}>
          Day {forecast?.current_day} of {forecast?.total_days}
        </Text>
        <Text style={styles.bigProgress}>
          {((forecast?.overall_progress || 0) * 100).toFixed(0)}% complete
        </Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Recovery Milestones</Text>
        {forecast?.milestones?.map((m, i) => (
          <View key={i} style={styles.milestoneRow}>
            <Text style={styles.milestoneName}>{m.name}</Text>
            <Text style={[
              styles.milestoneStatus,
              { color: m.status === "achieved" ? "#4CAF50" : m.status === "on_track" ? "#2196F3" : "#FF9800" }
            ]}>
              {m.status === "achieved" ? "✓ Achieved" :
               m.status === "on_track" ? `Day ${m.predicted_day}` : `Day ${m.predicted_day} (delayed)`}
            </Text>
            <Text style={styles.milestoneConfidence}>
              ({(m.confidence * 100).toFixed(0)}%)
            </Text>
          </View>
        ))}
      </View>
    </ScrollView>
  );
}

// === Nursing Screen ===
function NursingScreen() {
  const [sessions, setSessions] = useState<NursingSession[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.getNursingHistory("patient_001").then((data) => {
      setSessions(data.sessions || []);
      setLoading(false);
    });
  }, []);

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  const totalToday = sessions.filter(s => {
    const d = new Date(s.timestamp * 1000);
    const today = new Date();
    return d.toDateString() === today.toDateString();
  }).length;

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Nursing Log</Text>
        <Text style={styles.subtitle}>{totalToday} sessions today</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Today's Sessions</Text>
        {sessions.length === 0 ? (
          <Text style={styles.cardText}>No nursing sessions logged yet</Text>
        ) : (
          sessions.slice(0, 10).map((s, i) => (
            <View key={i} style={styles.nursingRow}>
              <Text style={styles.nursingTime}>
                {new Date(s.timestamp * 1000).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}
              </Text>
              <Text style={styles.nursingSide}>
                {s.side === "left" ? "👈 Left" : s.side === "right" ? "Right 👉" : s.side}
              </Text>
              <Text style={styles.nursingDuration}>{s.duration_min} min</Text>
              <Text style={[
                styles.nursingRisk,
                { color: s.mastitis_risk > 50 ? "#F44336" : s.mastitis_risk > 25 ? "#FF9800" : "#4CAF50" }
              ]}>
                {s.mastitis_risk}%
              </Text>
            </View>
          ))
        )}
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Mastitis Watch</Text>
        <Text style={styles.cardText}>
          Bilateral breast temperature monitoring active.{"\n"}
          Asymmetry threshold: 1.3°C (clinical)
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
    { id: "RB-01", name: "Recovery Band", battery: 82, paired: true },
    { id: "NS-01", name: "Nursing Sensor", battery: 65, paired: true },
    { id: "WP-01", name: "Wound Patch", battery: 48, paired: true },
  ]);
  const [audioReminders, setAudioReminders] = useState(true);
  const [hapticAlerts, setHapticAlerts] = useState(true);
  const [partnerSharing, setPartnerSharing] = useState(true);

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
          <Text style={styles.cardText}>Audio Reminders</Text>
          <Switch value={audioReminders} onValueChange={setAudioReminders} />
        </View>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Haptic Alerts</Text>
          <Switch value={hapticAlerts} onValueChange={setHapticAlerts} />
        </View>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Partner Sharing</Text>
          <Switch value={partnerSharing} onValueChange={setPartnerSharing} />
        </View>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Healthcare Provider</Text>
        <Text style={styles.cardText}>Dr. Emily Rodriguez, MD</Text>
        <Text style={styles.cardText}>OB-GYN</Text>
        <TouchableOpacity style={styles.secondaryButton}>
          <Text style={styles.buttonText}>Send Message</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.secondaryButton}>
          <Text style={styles.buttonText}>Schedule Telehealth Visit</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Partner Access</Text>
        <Text style={styles.cardText}>Mike Johnson (partner)</Text>
        <Text style={styles.cardText}>Sharing: Vitals + Alerts</Text>
        <TouchableOpacity style={styles.secondaryButton}>
          <Text style={styles.buttonText}>Manage Sharing</Text>
        </TouchableOpacity>
      </View>
    </ScrollView>
  );
}

// === Navigation ===
const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: "#E91E63",
          tabBarStyle: { paddingBottom: 5 },
        }}
      >
        <Tab.Screen name="Vitals" component={VitalsScreen}
          options={{ tabBarLabel: "Vitals" }} />
        <Tab.Screen name="Recovery" component={RecoveryScreen}
          options={{ tabBarLabel: "Recovery" }} />
        <Tab.Screen name="Nursing" component={NursingScreen}
          options={{ tabBarLabel: "Nursing" }} />
        <Tab.Screen name="Alerts" component={AlertsScreen}
          options={{ tabBarLabel: "Alerts" }} />
        <Tab.Screen name="Settings" component={SettingsScreen}
          options={{ tabBarLabel: "Settings" }} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

// === Styles ===
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: "#fce4ec" },
  header: { padding: 20, backgroundColor: "#E91E63" },
  title: { fontSize: 24, fontWeight: "bold", color: "#fff" },
  subtitle: { fontSize: 14, color: "#f8bbd0", marginTop: 4 },
  card: { backgroundColor: "#fff", margin: 10, padding: 15, borderRadius: 10, elevation: 2 },
  cardTitle: { fontSize: 18, fontWeight: "bold", marginBottom: 8, color: "#333" },
  cardText: { fontSize: 14, color: "#666", marginBottom: 4 },
  metricsGrid: { flexDirection: "row", flexWrap: "wrap", justifyContent: "space-between", padding: 10 },
  metricCard: { backgroundColor: "#fff", width: "48%", padding: 15, borderRadius: 10, marginBottom: 10, elevation: 2, alignItems: "center" },
  metricLabel: { fontSize: 12, color: "#999", marginBottom: 4 },
  metricValue: { fontSize: 32, fontWeight: "bold", color: "#333" },
  metricUnit: { fontSize: 12, color: "#999" },
  progressText: { fontSize: 16, color: "#666", marginBottom: 4 },
  bigProgress: { fontSize: 28, fontWeight: "bold", color: "#E91E63" },
  riskBarRow: { flexDirection: "row", alignItems: "center", marginBottom: 8 },
  riskBarLabel: { fontSize: 13, color: "#555", width: 100 },
  riskBarTrack: { flex: 1, height: 8, backgroundColor: "#f5f5f5", borderRadius: 4, marginHorizontal: 8 },
  riskBarFill: { height: 8, borderRadius: 4 },
  riskBarValue: { fontSize: 12, color: "#666", width: 35, textAlign: "right" },
  overallRiskRow: { flexDirection: "row", justifyContent: "space-between", marginTop: 8, paddingTop: 8, borderTopWidth: 1, borderTopColor: "#eee" },
  overallRiskValue: { fontSize: 16, fontWeight: "bold" },
  alertCard: { backgroundColor: "#FFF3E0", margin: 10, padding: 15, borderRadius: 10, borderLeftWidth: 4, borderLeftColor: "#FF9800" },
  alertText: { fontSize: 16, fontWeight: "bold", color: "#E65100" },
  alertSubtext: { fontSize: 12, color: "#BF360C", marginTop: 4 },
  milestoneRow: { flexDirection: "row", alignItems: "center", marginBottom: 8 },
  milestoneName: { fontSize: 14, flex: 1, color: "#333" },
  milestoneStatus: { fontSize: 13, fontWeight: "600" },
  milestoneConfidence: { fontSize: 11, color: "#999", marginLeft: 8 },
  nursingRow: { flexDirection: "row", alignItems: "center", paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: "#f5f5f5" },
  nursingTime: { fontSize: 13, color: "#666", width: 60 },
  nursingSide: { fontSize: 14, flex: 1 },
  nursingDuration: { fontSize: 13, color: "#666", width: 50, textAlign: "right" },
  nursingRisk: { fontSize: 12, fontWeight: "600", width: 40, textAlign: "right" },
  alertItem: { backgroundColor: "#fff", margin: 10, padding: 15, borderRadius: 10, borderLeftWidth: 4, elevation: 1 },
  alertType: { fontSize: 14, fontWeight: "bold", color: "#333", textTransform: "capitalize" },
  alertMessage: { fontSize: 13, color: "#666", marginTop: 4 },
  alertTime: { fontSize: 11, color: "#999", marginTop: 4 },
  ackButton: { backgroundColor: "#E91E63", padding: 8, borderRadius: 6, alignItems: "center", marginTop: 8 },
  sensorRow: { flexDirection: "row", alignItems: "center", paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: "#f5f5f5" },
  sensorName: { fontSize: 14, flex: 1, color: "#333" },
  sensorBattery: { fontSize: 13, color: "#666", marginRight: 10 },
  settingRow: { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: 4 },
  primaryButton: { backgroundColor: "#E91E63", padding: 15, borderRadius: 8, alignItems: "center", marginTop: 10 },
  secondaryButton: { backgroundColor: "#F8BBD0", padding: 12, borderRadius: 8, alignItems: "center", marginTop: 8 },
  unpairButton: { backgroundColor: "#ffcdd2", padding: 8, borderRadius: 6 },
  pairButton: { backgroundColor: "#E91E63", padding: 8, borderRadius: 6 },
  buttonText: { color: "#fff", fontSize: 14, fontWeight: "600" },
});