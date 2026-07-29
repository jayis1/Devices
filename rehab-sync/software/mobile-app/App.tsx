/*
 * RehabSync — Mobile App (React Native)
 * AI-powered physical therapy & post-surgery rehabilitation companion app.
 *
 * Features:
 * - Real-time session view (exercise, reps, form score, joint angles)
 * - Exercise plan with video demos
 * - Session history & calendar
 * - Recovery timeline forecast
 * - Adherence dashboard (streaks, completion rate)
 * - Form score trends & ROM progress charts
 * - Therapist communication (messaging, video calls)
 * - Push notifications (session reminders, alerts, milestones)
 * - Sensor management (pair/unpair, battery, calibration)
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
  ProgressViewIOS,
  ActivityIndicator,
  RefreshControl,
  Platform,
} from "react-native";
import { NavigationContainer } from "@react-navigation/native";
import { createBottomTabNavigator } from "@react-navigation/bottom-tabs";

const API_BASE = "https://api.rehab-sync.io/api/v1";
const WS_BASE = "wss://api.rehab-sync.io/ws/realtime";

// === Types ===
interface Exercise {
  id: number;
  name: string;
  category: string;
  target_joints: string[];
  sets: number;
  reps: number;
  resistance_kg: number;
  frequency_per_day: number;
}

interface SessionData {
  id: string;
  patient_id: string;
  exercise: number;
  reps: number;
  form_score: number;
  form_deviation: number;
  joint_angles: Record<string, number>;
  force_mg: number;
  weight_g: number;
  asymmetry: number;
  sensors_connected: number;
  band_connected: boolean;
  mat_connected: boolean;
}

interface RecoveryForecast {
  patient_id: string;
  current_week: number;
  milestones: Array<{
    name: string;
    target_days: number;
    predicted_days: number;
    status: string;
    confidence: number;
  }>;
  overall_progress: number;
  adherence_rate: number;
  avg_form_score: number;
  risk_flags: string[];
}

interface AdherenceData {
  patient_id: string;
  last_7_days: Array<{
    date: string;
    sessions: number;
    completed: number;
    duration_min: number;
  }>;
  streak_days: number;
  completion_rate_7d: number;
  dropout_risk_7d: number;
  avg_sessions_per_day: number;
}

// === API Service ===
const api = {
  async getExercisePlan(patientId: string) {
    const res = await fetch(`${API_BASE}/exercise-plans/${patientId}`);
    return res.json();
  },

  async getRecoveryForecast(patientId: string): Promise<RecoveryForecast> {
    const res = await fetch(`${API_BASE}/recovery-forecast/${patientId}`);
    return res.json();
  },

  async getAdherence(patientId: string): Promise<AdherenceData> {
    const res = await fetch(`${API_BASE}/adherence/${patientId}`);
    return res.json();
  },

  async getFormTrends(patientId: string) {
    const res = await fetch(`${API_BASE}/form-trends/${patientId}`);
    return res.json();
  },

  async getRomProgress(patientId: string) {
    const res = await fetch(`${API_BASE}/rom-progress/${patientId}`);
    return res.json();
  },

  async startSession(patientId: string, planId: string) {
    const res = await fetch(`${API_BASE}/sessions`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        patient_id: patientId,
        exercise_plan_id: planId,
        scheduled_duration_min: 30,
      }),
    });
    return res.json();
  },

  async endSession(sessionId: string) {
    const res = await fetch(`${API_BASE}/sessions/${sessionId}/end`, { method: "POST" });
    return res.json();
  },

  async getSessions(patientId: string) {
    const res = await fetch(`${API_BASE}/sessions?patient_id=${patientId}`);
    return res.json();
  },
};

// === Session Screen (real-time exercise view) ===
function SessionScreen() {
  const [sessionActive, setSessionActive] = useState(false);
  const [sessionId, setSessionId] = useState<string | null>(null);
  const [currentExercise, setCurrentExercise] = useState(0);
  const [reps, setReps] = useState(0);
  const [formScore, setFormScore] = useState(100);
  const [jointAngle, setJointAngle] = useState(0);
  const [force, setForce] = useState(0);
  const [weight, setWeight] = useState(0);
  const [asymmetry, setAsymmetry] = useState(0);
  const [sensorsConnected, setSensorsConnected] = useState(0);
  const [ws, setWs] = useState<WebSocket | null>(null);

  const startSession = async () => {
    const result = await api.startSession("patient_001", "plan_001");
    setSessionId(result.session_id);
    setSessionActive(true);
    setReps(0);

    // Connect WebSocket for real-time data
    const websocket = new WebSocket(`${WS_BASE}/patient_001`);
    websocket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      if (data.type === "telemetry") {
        const t = data.data;
        setReps(t.reps);
        setFormScore(t.form_score);
        setJointAngle(t.joint_angles?.knee_flexion || 0);
        setForce(Math.abs(t.force_mg) / 1000); // mg to kg
        setWeight(t.weight_g / 1000); // g to kg
        setAsymmetry(t.asymmetry);
        setSensorsConnected(t.sensors_connected);
      }
    };
    setWs(websocket);
  };

  const stopSession = async () => {
    if (sessionId) {
      await api.endSession(sessionId);
    }
    setSessionActive(false);
    setSessionId(null);
    ws?.close();
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Exercise Session</Text>
      </View>

      {!sessionActive ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Ready to Start?</Text>
          <Text style={styles.cardText}>
            Put on your body sensors and connect your smart band.
            {"\n"}Sensors connected: {sensorsConnected}
          </Text>
          <TouchableOpacity style={styles.primaryButton} onPress={startSession}>
            <Text style={styles.buttonText}>Start Session</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <View>
          {/* Real-time metrics */}
          <View style={styles.metricsGrid}>
            <View style={styles.metricCard}>
              <Text style={styles.metricLabel}>Reps</Text>
              <Text style={styles.metricValue}>{reps}</Text>
            </View>
            <View style={styles.metricCard}>
              <Text style={styles.metricLabel}>Form Score</Text>
              <Text style={[styles.metricValue, { color: formScore >= 80 ? "#4CAF50" : formScore >= 60 ? "#FF9800" : "#F44336" }]}>
                {formScore}
              </Text>
            </View>
            <View style={styles.metricCard}>
              <Text style={styles.metricLabel}>Knee Angle</Text>
              <Text style={styles.metricValue}>{jointAngle.toFixed(0)}°</Text>
            </View>
            <View style={styles.metricCard}>
              <Text style={styles.metricLabel}>Force</Text>
              <Text style={styles.metricValue}>{force.toFixed(1)} kg</Text>
            </View>
          </View>

          {/* Balance metrics */}
          {weight > 0 && (
            <View style={styles.card}>
              <Text style={styles.cardTitle}>Balance & Weight-Bearing</Text>
              <Text style={styles.cardText}>Total weight: {weight.toFixed(1)} kg</Text>
              <Text style={styles.cardText}>L/R Asymmetry: {(asymmetry / 10).toFixed(1)}%</Text>
            </View>
          )}

          {/* Form feedback */}
          {formScore < 60 && (
            <View style={styles.alertCard}>
              <Text style={styles.alertText}>⚠️ Form correction needed</Text>
              <Text style={styles.alertSubtext}>Listen to audio coaching from your Hub</Text>
            </View>
          )}

          <TouchableOpacity style={styles.stopButton} onPress={stopSession}>
            <Text style={styles.buttonText}>End Session</Text>
          </TouchableOpacity>
        </View>
      )}
    </ScrollView>
  );
}

// === Plan Screen (exercise plan) ===
function PlanScreen() {
  const [plan, setPlan] = useState<any>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.getExercisePlan("patient_001").then((data) => {
      setPlan(data);
      setLoading(false);
    });
  }, []);

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Today's Exercises</Text>
      </View>
      {plan?.exercises?.map((ex: any, i: number) => (
        <View key={i} style={styles.card}>
          <Text style={styles.cardTitle}>{ex.name}</Text>
          <Text style={styles.cardText}>
            {ex.sets} sets × {ex.reps} reps
            {ex.resistance_kg > 0 ? ` @ ${ex.resistance_kg} kg` : ""}
            {"\n"}{ex.frequency_per_day}× per day
          </Text>
          <TouchableOpacity style={styles.secondaryButton}>
            <Text style={styles.buttonText}>View Demo</Text>
          </TouchableOpacity>
        </View>
      ))}
    </ScrollView>
  );
}

// === Progress Screen (recovery timeline + trends) ===
function ProgressScreen() {
  const [forecast, setForecast] = useState<RecoveryForecast | null>(null);
  const [adherence, setAdherence] = useState<AdherenceData | null>(null);
  const [formTrends, setFormTrends] = useState<any[]>([]);
  const [romProgress, setRomProgress] = useState<any[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    Promise.all([
      api.getRecoveryForecast("patient_001"),
      api.getAdherence("patient_001"),
      api.getFormTrends("patient_001"),
      api.getRomProgress("patient_001"),
    ]).then(([f, a, ft, rom]) => {
      setForecast(f);
      setAdherence(a);
      setFormTrends(ft.trends || []);
      setRomProgress(rom.progress || []);
      setLoading(false);
    });
  }, []);

  if (loading) return <ActivityIndicator style={styles.container} size="large" />;

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Recovery Progress</Text>
      </View>

      {/* Overall progress */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Overall Progress</Text>
        <Text style={styles.cardText}>Week {forecast?.current_week} of 8</Text>
        <Text style={styles.progressText}>
          {((forecast?.overall_progress || 0) * 100).toFixed(0)}% complete
        </Text>
        <Text style={styles.cardText}>
          Adherence: {((forecast?.adherence_rate || 0) * 100).toFixed(0)}% | 
          Avg Form: {forecast?.avg_form_score}/100
        </Text>
      </View>

      {/* Milestones */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Recovery Milestones</Text>
        {forecast?.milestones?.map((m, i) => (
          <View key={i} style={styles.milestoneRow}>
            <Text style={styles.milestoneName}>{m.name}</Text>
            <Text style={[
              styles.milestoneStatus,
              { color: m.status === "achieved" ? "#4CAF50" : m.status === "on_track" ? "#2196F3" : "#FF9800" }
            ]}>
              {m.status === "achieved" ? "✓" : `${m.predicted_days}d`}
            </Text>
            <Text style={styles.milestoneConfidence}>
              ({(m.confidence * 100).toFixed(0)}%)
            </Text>
          </View>
        ))}
      </View>

      {/* Adherence */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Adherence (7 days)</Text>
        <Text style={styles.cardText}>
          Streak: {adherence?.streak_days} days | 
          Completion: {((adherence?.completion_rate_7d || 0) * 100).toFixed(0)}%
        </Text>
        <Text style={styles.cardText}>
          Dropout risk: {((adherence?.dropout_risk_7d || 0) * 100).toFixed(0)}%
        </Text>
      </View>

      {/* ROM progress */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Range of Motion</Text>
        {romProgress.length > 0 && (
          <Text style={styles.cardText}>
            Knee Flexion: {romProgress[romProgress.length - 1]?.knee_flexion}° →
            Target: 115°
          </Text>
        )}
      </View>

      {/* Form score trend */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Form Score Trend</Text>
        {formTrends.map((t, i) => (
          <Text key={i} style={styles.cardText}>
            {t.date}: {t.avg_form_score}/100 ({t.exercises} exercises)
          </Text>
        ))}
      </View>
    </ScrollView>
  );
}

// === Settings Screen (sensor management) ===
function SettingsScreen() {
  const [sensors, setSensors] = useState([
    { id: "BS-01", name: "Body Sensor (Thigh)", battery: 85, paired: true },
    { id: "BS-02", name: "Body Sensor (Shin)", battery: 78, paired: true },
    { id: "BS-03", name: "Body Sensor (Upper Arm)", battery: 92, paired: false },
    { id: "SB-01", name: "Smart Band", battery: 65, paired: true },
    { id: "PM-01", name: "Pressure Mat", battery: 100, paired: true },
  ]);
  const [audioCoaching, setAudioCoaching] = useState(true);
  const [hapticFeedback, setHapticFeedback] = useState(true);

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
                setSensors(sensors.map(x =>
                  x.id === s.id ? { ...x, paired: !x.paired } : x
                ));
              }}
            >
              <Text style={styles.buttonText}>{s.paired ? "Unpair" : "Pair"}</Text>
            </TouchableOpacity>
          </View>
        ))}
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Feedback</Text>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Audio Coaching</Text>
          <Switch value={audioCoaching} onValueChange={setAudioCoaching} />
        </View>
        <View style={styles.settingRow}>
          <Text style={styles.cardText}>Haptic Feedback</Text>
          <Switch value={hapticFeedback} onValueChange={setHapticFeedback} />
        </View>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Therapist</Text>
        <Text style={styles.cardText}>Dr. Sarah Chen, DPT</Text>
        <TouchableOpacity style={styles.secondaryButton}>
          <Text style={styles.buttonText}>Send Message</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.secondaryButton}>
          <Text style={styles.buttonText}>Schedule Video Call</Text>
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
          tabBarActiveTintColor: "#2196F3",
          tabBarStyle: { paddingBottom: 5 },
        }}
      >
        <Tab.Screen name="Session" component={SessionScreen}
          options={{ tabBarLabel: "Exercise" }} />
        <Tab.Screen name="Plan" component={PlanScreen}
          options={{ tabBarLabel: "Plan" }} />
        <Tab.Screen name="Progress" component={ProgressScreen}
          options={{ tabBarLabel: "Progress" }} />
        <Tab.Screen name="Settings" component={SettingsScreen}
          options={{ tabBarLabel: "Settings" }} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

// === Styles ===
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: "#f5f5f5" },
  header: { padding: 20, backgroundColor: "#2196F3" },
  title: { fontSize: 24, fontWeight: "bold", color: "#fff" },
  card: { backgroundColor: "#fff", margin: 10, padding: 15, borderRadius: 10, elevation: 2 },
  cardTitle: { fontSize: 18, fontWeight: "bold", marginBottom: 8 },
  cardText: { fontSize: 14, color: "#666", marginBottom: 4 },
  metricsGrid: { flexDirection: "row", flexWrap: "wrap", justifyContent: "space-between", padding: 10 },
  metricCard: { backgroundColor: "#fff", width: "48%", padding: 15, borderRadius: 10, marginBottom: 10, elevation: 2, alignItems: "center" },
  metricLabel: { fontSize: 12, color: "#999", marginBottom: 4 },
  metricValue: { fontSize: 28, fontWeight: "bold", color: "#333" },
  alertCard: { backgroundColor: "#FFF3E0", margin: 10, padding: 15, borderRadius: 10, borderLeftWidth: 4, borderLeftColor: "#FF9800" },
  alertText: { fontSize: 16, fontWeight: "bold", color: "#E65100" },
  alertSubtext: { fontSize: 12, color: "#BF360C", marginTop: 4 },
  primaryButton: { backgroundColor: "#2196F3", padding: 15, borderRadius: 8, alignItems: "center", marginTop: 10 },
  secondaryButton: { backgroundColor: "#E3F2FD", padding: 12, borderRadius: 8, alignItems: "center", marginTop: 8 },
  stopButton: { backgroundColor: "#F44336", padding: 15, borderRadius: 8, alignItems: "center", margin: 10 },
  buttonText: { color: "#fff", fontSize: 16, fontWeight: "600" },
  progressText: { fontSize: 32, fontWeight: "bold", color: "#2196F3", marginVertical: 8 },
  milestoneRow: { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: "#eee" },
  milestoneName: { fontSize: 14, flex: 1 },
  milestoneStatus: { fontSize: 16, fontWeight: "bold" },
  milestoneConfidence: { fontSize: 12, color: "#999", width: 50, textAlign: "right" },
  sensorRow: { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: 10, borderBottomWidth: 1, borderBottomColor: "#eee" },
  sensorName: { fontSize: 14, flex: 1 },
  sensorBattery: { fontSize: 12, color: "#999", width: 60 },
  pairButton: { backgroundColor: "#4CAF50", padding: 8, borderRadius: 6 },
  unpairButton: { backgroundColor: "#f44336", padding: 8, borderRadius: 6 },
  settingRow: { flexDirection: "row", justifyContent: "space-between", alignItems: "center", paddingVertical: 8 },
});