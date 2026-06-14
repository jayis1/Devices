/**
 * App.tsx — SleepSync Mobile App (React Native)
 *
 * Navigation: Home → SleepScore, Environment, Soundscape, Alarm, Report, SetupWizard
 */

import React, {useState, useEffect} from 'react';
import {NavigationContainer} from '@react-navigation/native';
import {createBottomTabNavigator} from '@react-navigation/bottom-tabs';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity,
  SafeAreaView, Alert, RefreshControl, Dimensions
} from 'react-native';

const {width} = Dimensions.get('window');

const API_BASE = 'http://192.168.1.100:8000/api';

const STAGE_NAMES = ['AWAKE', 'LIGHT', 'DEEP', 'REM'];
const STAGE_COLORS = ['#F44336', '#2196F3', '#9C27B0', '#4CAF50'];
const SOUND_NAMES = ['Off', 'White Noise', 'Pink Noise', 'Brown Noise',
                     'Rain', 'Ocean Waves', 'Forest', 'Campfire'];

// ---- Sleep Score Screen ----
function SleepScoreScreen() {
  const [score, setScore] = useState(null);
  const [sleepData, setSleepData] = useState(null);
  const [loading, setLoading] = useState(true);

  const fetchData = async () => {
    try {
      const [scoreResp, sleepResp] = await Promise.all([
        fetch(`${API_BASE}/sleep/score`),
        fetch(`${API_BASE}/sleep/latest`),
      ]);
      setScore(await scoreResp.json());
      setSleepData(await sleepResp.json());
    } catch (e) {
      console.error('Fetch failed:', e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchData(); const iv = setInterval(fetchData, 10000); return () => clearInterval(iv); }, []);

  const stageIdx = sleepData?.sleep_stage ?? 0;

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>SleepSync</Text>

      {/* Sleep Score Circle */}
      <View style={styles.scoreCircle}>
        <Text style={[styles.scoreValue, {color: score?.score > 70 ? '#4CAF50' : '#FF9800'}]}>
          {score?.score?.toFixed(0) ?? '--'}
        </Text>
        <Text style={styles.scoreLabel}>Sleep Score</Text>
      </View>

      {/* Current Stage */}
      <View style={[styles.stageCard, {borderLeftColor: STAGE_COLORS[stageIdx]}]}>
        <Text style={styles.stageLabel}>Current Stage</Text>
        <Text style={[styles.stageValue, {color: STAGE_COLORS[stageIdx]}]}>
          {STAGE_NAMES[stageIdx]}
        </Text>
      </View>

      {/* Live Vitals */}
      {sleepData && (
        <View style={styles.vitalsCard}>
          <Text style={styles.cardTitle}>Live Vitals</Text>
          <View style={styles.vitalRow}>
            <Text style={styles.vitalLabel}>Heart Rate</Text>
            <Text style={styles.vitalValue}>{(sleepData.heart_rate / 10).toFixed(1)} BPM</Text>
          </View>
          <View style={styles.vitalRow}>
            <Text style={styles.vitalLabel}>Respiration</Text>
            <Text style={styles.vitalValue}>{(sleepData.resp_rate / 10).toFixed(1)} /min</Text>
          </View>
          <View style={styles.vitalRow}>
            <Text style={styles.vitalLabel}>Movement</Text>
            <Text style={styles.vitalValue}>{sleepData.movement}</Text>
          </View>
          <View style={styles.vitalRow}>
            <Text style={styles.vitalLabel}>Snoring</Text>
            <Text style={[styles.vitalValue, {color: sleepData.snoring > 100 ? '#F44336' : '#4CAF50'}]}>
              {sleepData.snoring > 100 ? 'Detected' : 'None'}
            </Text>
          </View>
        </View>
      )}

      {/* Stage Distribution */}
      {score?.stages && (
        <View style={styles.vitalsCard}>
          <Text style={styles.cardTitle}>Tonight's Stages</Text>
          {['deep_pct', 'rem_pct', 'light_pct', 'wake_pct'].map((key, i) => (
            <View key={key} style={styles.barRow}>
              <Text style={styles.barLabel}>
                {['Deep', 'REM', 'Light', 'Wake'][i]}
              </Text>
              <View style={styles.barTrack}>
                <View style={[styles.barFill, {
                  width: `${score.stages[key]}%`,
                  backgroundColor: STAGE_COLORS[[2,3,1,0][i]]
                }]} />
              </View>
              <Text style={styles.barValue}>{score.stages[key].toFixed(1)}%</Text>
            </View>
          ))}
        </View>
      )}
    </SafeAreaView>
  );
}

// ---- Environment Screen ----
function EnvironmentScreen() {
  const [env, setEnv] = useState(null);
  const [recs, setRecs] = useState(null);

  useEffect(() => {
    fetch(`${API_BASE}/env/latest`).then(r => r.json()).then(setEnv).catch(console.error);
    fetch(`${API_BASE}/env/recommendations`).then(r => r.json()).then(setRecs).catch(console.error);
  }, []);

  const setClimate = async (temp: number, hum: number) => {
    try {
      await fetch(`${API_BASE}/climate/setpoint`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({temperature: temp, humidity: hum}),
      });
      Alert.alert('Set!', `Target: ${temp}°C, ${hum}% RH`);
    } catch { Alert.alert('Error', 'Failed to set climate'); }
  };

  const currentTemp = env?.temperature ? (env.temperature / 100).toFixed(1) : '--';
  const currentHum = env?.humidity ? (env.humidity / 100).toFixed(1) : '--';
  const currentCO2 = env?.co2_ppm ?? '--';

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Environment</Text>

      <View style={styles.envGrid}>
        <View style={styles.envCard}>
          <Text style={styles.envValue}>{currentTemp}°C</Text>
          <Text style={styles.envLabel}>Temperature</Text>
          <Text style={styles.envRange}>
            Optimal: {recs?.population_optimal?.temperature?.min}–{recs?.population_optimal?.temperature?.max}°C
          </Text>
        </View>
        <View style={styles.envCard}>
          <Text style={styles.envValue}>{currentHum}%</Text>
          <Text style={styles.envLabel}>Humidity</Text>
          <Text style={styles.envRange}>
            Optimal: {recs?.population_optimal?.humidity?.min}–{recs?.population_optimal?.humidity?.max}%
          </Text>
        </View>
        <View style={styles.envCard}>
          <Text style={styles.envValue}>{currentCO2}</Text>
          <Text style={styles.envLabel}>CO₂ (ppm)</Text>
          <Text style={styles.envRange}>Optimal: &lt;800</Text>
        </View>
      </View>

      <View style={styles.controlCard}>
        <Text style={styles.cardTitle}>Quick Setpoints</Text>
        <View style={styles.setpointButtons}>
          <TouchableOpacity style={styles.setpointBtn} onPress={() => setClimate(19.0, 45)}>
            <Text style={styles.setpointBtnText}>Cool 19°C</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.setpointBtn} onPress={() => setClimate(20.0, 45)}>
            <Text style={styles.setpointBtnText}>Mild 20°C</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.setpointBtn} onPress={() => setClimate(21.0, 45)}>
            <Text style={styles.setpointBtnText}>Warm 21°C</Text>
          </TouchableOpacity>
        </View>
      </View>

      {/* Shade Control */}
      <View style={styles.controlCard}>
        <Text style={styles.cardTitle}>Window Shade</Text>
        <View style={styles.setpointButtons}>
          <TouchableOpacity style={styles.shadeBtn} onPress={() => {
            fetch(`${API_BASE}/shade/position`, {
              method: 'POST', headers: {'Content-Type': 'application/json'},
              body: JSON.stringify({position: 0})
            });
          }}>
            <Text style={styles.setpointBtnText}>Close</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.shadeBtn} onPress={() => {
            fetch(`${API_BASE}/shade/position`, {
              method: 'POST', headers: {'Content-Type': 'application/json'},
              body: JSON.stringify({position: 50})
            });
          }}>
            <Text style={styles.setpointBtnText}>50%</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.shadeBtn} onPress={() => {
            fetch(`${API_BASE}/shade/position`, {
              method: 'POST', headers: {'Content-Type': 'application/json'},
              body: JSON.stringify({position: 100})
            });
          }}>
            <Text style={styles.setpointBtnText}>Open</Text>
          </TouchableOpacity>
        </View>
      </View>
    </SafeAreaView>
  );
}

// ---- Soundscape Screen ----
function SoundscapeScreen() {
  const [selected, setSelected] = useState(1);
  const [volume, setVolume] = useState(80);

  const setSound = async (id: number) => {
    setSelected(id);
    try {
      await fetch(`${API_BASE}/sound`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({sound_id: id, volume}),
      });
    } catch {}
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Soundscape</Text>
      <ScrollView>
        {SOUND_NAMES.map((name, i) => (
          <TouchableOpacity key={i} style={[styles.soundCard, selected === i && styles.soundSelected]}
            onPress={() => setSound(i)}>
            <Text style={[styles.soundName, selected === i && styles.soundNameSelected]}>{name}</Text>
            {selected === i && <Text style={styles.soundCheck}>✓</Text>}
          </TouchableOpacity>
        ))}

        {/* Volume Control (placeholder — in production use Slider) */}
        <View style={styles.volumeCard}>
          <Text style={styles.cardTitle}>Volume</Text>
          <View style={styles.volumeRow}>
            {['🔇', '🔈', '🔉', '🔊'].map((emoji, i) => (
              <TouchableOpacity key={i} style={styles.volBtn}
                onPress={() => { setVolume(i * 64 + 32); setSound(selected); }}>
                <Text style={styles.volEmoji}>{emoji}</Text>
              </TouchableOpacity>
            ))}
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Smart Alarm Screen ----
function SmartAlarmScreen() {
  const [alarmStart, setAlarmStart] = useState('06:30');
  const [alarmEnd, setAlarmEnd] = useState('07:00');
  const [enabled, setEnabled] = useState(true);

  const saveAlarm = async () => {
    try {
      await fetch(`${API_BASE}/alarm`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          window_start: alarmStart,
          window_end: alarmEnd,
          enabled,
        }),
      });
      Alert.alert('Saved!', `Smart alarm: ${alarmStart} – ${alarmEnd}`);
    } catch { Alert.alert('Error', 'Failed to save alarm'); }
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Smart Alarm</Text>

      <View style={styles.alarmCard}>
        <Text style={styles.cardTitle}>Wake Window</Text>
        <Text style={styles.alarmDesc}>
          SleepSync will wake you at the lightest sleep point within this window.
        </Text>

        <View style={styles.alarmTimeRow}>
          <TouchableOpacity style={styles.timeBtn}
            onPress={() => setAlarmStart('06:00')}>
            <Text style={styles.timeText}>{alarmStart}</Text>
            <Text style={styles.timeLabel}>Earliest</Text>
          </TouchableOpacity>
          <Text style={styles.timeDash}>—</Text>
          <TouchableOpacity style={styles.timeBtn}
            onPress={() => setAlarmEnd('07:00')}>
            <Text style={styles.timeText}>{alarmEnd}</Text>
            <Text style={styles.timeLabel}>Latest</Text>
          </TouchableOpacity>
        </View>

        {/* Quick presets */}
        <View style={styles.presetRow}>
          {[
            {label: 'Early', start: '05:30', end: '06:00'},
            {label: 'Normal', start: '06:30', end: '07:00'},
            {label: 'Late', start: '07:30', end: '08:00'},
            {label: 'Weekend', start: '08:30', end: '09:00'},
          ].map(p => (
            <TouchableOpacity key={p.label} style={styles.presetBtn}
              onPress={() => { setAlarmStart(p.start); setAlarmEnd(p.end); }}>
              <Text style={styles.presetText}>{p.label}</Text>
            </TouchableOpacity>
          ))}
        </View>

        <TouchableOpacity style={styles.saveBtn} onPress={saveAlarm}>
          <Text style={styles.saveBtnText}>Save Alarm</Text>
        </TouchableOpacity>
      </View>

      {/* Dawn Simulation info */}
      <View style={styles.infoCard}>
        <Text style={styles.infoTitle}>🌅 Dawn Simulation</Text>
        <Text style={styles.infoText}>
          30 minutes before your alarm window, the shade controller begins a gradual sunrise:
          amber → warm white → cool white. You wake naturally to light, not sound.
        </Text>
      </View>
    </SafeAreaView>
  );
}

// ---- Weekly Report Screen ----
function ReportScreen() {
  const [report, setReport] = useState(null);

  useEffect(() => {
    fetch(`${API_BASE}/report/daily`).then(r => r.json()).then(setReport).catch(console.error);
    fetch(`${API_BASE}/health/apnea_risk`).then(r => r.json()).then(d => console.log('Apnea:', d)).catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Sleep Report</Text>
      <ScrollView>
        {report?.sleep_score != null ? (
          <View style={styles.reportCard}>
            <Text style={styles.reportScore}>{report.sleep_score?.toFixed(0)}</Text>
            <Text style={styles.reportLabel}>Last Night's Score</Text>

            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>Deep Sleep</Text>
              <Text style={styles.reportValue}>{report.deep_sleep_pct?.toFixed(1)}%</Text>
            </View>
            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>REM Sleep</Text>
              <Text style={styles.reportValue}>{report.rem_sleep_pct?.toFixed(1)}%</Text>
            </View>
            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>Sleep Latency</Text>
              <Text style={styles.reportValue}>{report.sleep_latency_min?.toFixed(0)} min</Text>
            </View>
            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>Wake Episodes</Text>
              <Text style={styles.reportValue}>{report.waso_count}</Text>
            </View>
            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>Snoring</Text>
              <Text style={styles.reportValue}>{report.snoring_min?.toFixed(0)} min</Text>
            </View>
            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>Avg Temperature</Text>
              <Text style={styles.reportValue}>{report.avg_temp?.toFixed(1)}°C</Text>
            </View>
            <View style={styles.reportRow}>
              <Text style={styles.reportMetric}>Avg Humidity</Text>
              <Text style={styles.reportValue}>{report.avg_humidity?.toFixed(1)}%</Text>
            </View>

            {report.recommendations && (
              <View style={styles.recBox}>
                <Text style={styles.recTitle}>💡 AI Recommendations</Text>
                <Text style={styles.recText}>{report.recommendations}</Text>
              </View>
            )}
          </View>
        ) : (
          <View style={styles.noData}>
            <Text style={styles.noDataText}>No report yet</Text>
            <Text style={styles.noDataSub}>Reports are generated after each night of sleep</Text>
          </View>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Setup Wizard ----
function SetupScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Setup Wizard</Text>
      <ScrollView>
        {[
          '1. Power on Nightstand Hub',
          '2. App discovers Hub via BLE',
          '3. Enter WiFi credentials (provisioned via BLE)',
          '4. Hub connects to WiFi + MQTT',
          '5. Place Sleep Strip under pillow',
          '6. Strip auto-joins BLE mesh',
          '7. Mount Climate Node on wall',
          '8. Learn your AC/heater IR remote codes',
          '9. Install Shade Controller on window',
          '10. Run shade calibration (auto-detect limits)',
          '11. Set your bedtime + alarm window',
          '12. Sleep! System learns from Night 1',
        ].map((step, i) => (
          <Text key={i} style={styles.wizardStep}>{step}</Text>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Navigation ----
const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator screenOptions={{headerShown: false}}>
        <Tab.Screen name="Sleep" component={SleepScoreScreen} />
        <Tab.Screen name="Env" component={EnvironmentScreen} />
        <Tab.Screen name="Sound" component={SoundscapeScreen} />
        <Tab.Screen name="Alarm" component={SmartAlarmScreen} />
        <Tab.Screen name="Report" component={ReportScreen} />
        <Tab.Screen name="Setup" component={SetupScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a0e1a', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#b39ddb', marginBottom: 16 },
  scoreCircle: { alignItems: 'center', marginBottom: 20 },
  scoreValue: { fontSize: 72, fontWeight: 'bold' },
  scoreLabel: { color: '#7986cb', fontSize: 14 },
  stageCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16, marginBottom: 12, borderLeftWidth: 4 },
  stageLabel: { color: '#9e9ebe', fontSize: 12 },
  stageValue: { fontSize: 24, fontWeight: 'bold' },
  vitalsCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardTitle: { color: '#b39ddb', fontSize: 16, fontWeight: 'bold', marginBottom: 8 },
  vitalRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 4, borderBottomWidth: 0.5, borderBottomColor: '#2a2e3e' },
  vitalLabel: { color: '#9e9ebe', fontSize: 14 },
  vitalValue: { fontSize: 16, fontWeight: 'bold', color: '#e0e0e0' },
  barRow: { flexDirection: 'row', alignItems: 'center', paddingVertical: 4 },
  barLabel: { color: '#9e9ebe', fontSize: 14, width: 50 },
  barTrack: { flex: 1, height: 8, backgroundColor: '#2a2e3e', borderRadius: 4, marginHorizontal: 8 },
  barFill: { height: 8, borderRadius: 4 },
  barValue: { color: '#e0e0e0', fontSize: 12, width: 45, textAlign: 'right' },
  envGrid: { flexDirection: 'row', flexWrap: 'wrap', gap: 8, marginBottom: 16 },
  envCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16, flex: 1, minWidth: '30%' },
  envValue: { fontSize: 28, fontWeight: 'bold', color: '#4fc3f7' },
  envLabel: { color: '#9e9ebe', fontSize: 12, marginTop: 4 },
  envRange: { color: '#7986cb', fontSize: 10, marginTop: 2 },
  controlCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16, marginBottom: 12 },
  setpointButtons: { flexDirection: 'row', gap: 8 },
  setpointBtn: { backgroundColor: '#1a237e', borderRadius: 8, padding: 12, flex: 1 },
  shadeBtn: { backgroundColor: '#004d40', borderRadius: 8, padding: 12, flex: 1 },
  setpointBtnText: { color: 'white', textAlign: 'center', fontWeight: 'bold', fontSize: 14 },
  soundCard: { backgroundColor: '#1a1e2e', borderRadius: 10, padding: 16, marginBottom: 8, flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  soundSelected: { backgroundColor: '#1a237e', borderWidth: 1, borderColor: '#b39ddb' },
  soundName: { color: '#9e9ebe', fontSize: 16 },
  soundNameSelected: { color: '#b39ddb', fontWeight: 'bold' },
  soundCheck: { color: '#4CAF50', fontSize: 18 },
  volumeCard: { backgroundColor: '#1a1e2e', borderRadius: 10, padding: 16, marginTop: 8 },
  volumeRow: { flexDirection: 'row', justifyContent: 'space-around' },
  volBtn: { padding: 12 },
  volEmoji: { fontSize: 28 },
  alarmCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16 },
  alarmDesc: { color: '#9e9ebe', fontSize: 13, marginBottom: 16 },
  alarmTimeRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'center', marginBottom: 16 },
  timeBtn: { alignItems: 'center', backgroundColor: '#1a237e', borderRadius: 10, padding: 16, flex: 1 },
  timeText: { color: '#b39ddb', fontSize: 36, fontWeight: 'bold' },
  timeLabel: { color: '#7986cb', fontSize: 11, marginTop: 4 },
  timeDash: { color: '#9e9ebe', fontSize: 28, marginHorizontal: 12 },
  presetRow: { flexDirection: 'row', gap: 8, marginBottom: 16 },
  presetBtn: { backgroundColor: '#2a2e3e', borderRadius: 8, padding: 8, flex: 1 },
  presetText: { color: '#9e9ebe', textAlign: 'center', fontSize: 12 },
  saveBtn: { backgroundColor: '#1a237e', borderRadius: 10, padding: 16 },
  saveBtnText: { color: 'white', textAlign: 'center', fontWeight: 'bold', fontSize: 16 },
  infoCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16, marginTop: 12 },
  infoTitle: { color: '#b39ddb', fontSize: 16, fontWeight: 'bold', marginBottom: 8 },
  infoText: { color: '#9e9ebe', fontSize: 13, lineHeight: 20 },
  reportCard: { backgroundColor: '#1a1e2e', borderRadius: 12, padding: 16 },
  reportScore: { fontSize: 56, fontWeight: 'bold', color: '#4CAF50', textAlign: 'center' },
  reportLabel: { color: '#7986cb', fontSize: 12, textAlign: 'center', marginBottom: 16 },
  reportRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 6, borderBottomWidth: 0.5, borderBottomColor: '#2a2e3e' },
  reportMetric: { color: '#9e9ebe', fontSize: 14 },
  reportValue: { color: '#e0e0e0', fontSize: 14, fontWeight: 'bold' },
  recBox: { backgroundColor: '#0d1b2a', borderRadius: 8, padding: 12, marginTop: 16 },
  recTitle: { color: '#b39ddb', fontSize: 14, fontWeight: 'bold', marginBottom: 6 },
  recText: { color: '#9e9ebe', fontSize: 13, lineHeight: 20 },
  noData: { alignItems: 'center', marginTop: 40 },
  noDataText: { color: '#9e9ebe', fontSize: 20 },
  noDataSub: { color: '#7986cb', fontSize: 13, marginTop: 8 },
  wizardStep: { color: '#9e9ebe', fontSize: 16, paddingVertical: 12, borderBottomWidth: 0.5, borderBottomColor: '#2a2e3e' },
});