/**
 * GrillSync — Mobile App (React Native)
 *
 * AI-powered smart grilling & BBQ safety system companion app.
 * Real-time multi-probe temperature monitoring, doneness countdown,
 * thermal heat map, smoke quality, alerts, cook history, and recipes.
 */
import React, { useState, useEffect, useCallback } from 'react';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity, Alert,
  SafeAreaView, StatusBar, FlatList, Modal, Switch, TextInput,
} from 'react-native';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

// === Types ===
interface Probe {
  probeId: number;
  meatType: number;
  tempTip: number;
  tempMid: number;
  tempSurface: number;
  tempAmbient: number;
  targetTemp: number;
  doneness: number;
  etaSeconds: number;
  batteryV: number;
}

interface AlertItem {
  id: string;
  timestamp: string;
  alertType: string;
  severity: string;
  acknowledged: boolean;
  data?: any;
}

interface SmokeData {
  pm25: number;
  vocIndex: number;
  quality: number;
  qualityName: string;
}

interface GrillData {
  surfaceMax: number;
  surfaceAvg: number;
  hotZones: number;
  gasPpm: number;
  gasLelPct: number;
  flareupRisk: number;
  flareupEtaMs: number;
}

// === Constants ===
const MEAT_NAMES = ['Beef', 'Pork', 'Chicken', 'Fish', 'Lamb', 'Veal', 'Game', 'Custom'];
const DONENESS_NAMES = ['Raw', 'Rare', 'MR', 'Medium', 'MW', 'Well'];
const DONENESS_COLORS = ['#555', '#c0392b', '#e74c3c', '#e67e22', '#f39c12', '#27ae60'];
const SMOKE_NAMES = ['Clean Blue', 'Dirty White', 'Creosote', 'Thin Blue', 'No Smoke'];
const SEVERITY_COLORS: Record<string, string> = {
  critical: '#e74c3c',
  high: '#e67e22',
  medium: '#f39c12',
  low: '#3498db',
};
const SEVERITY_ICONS: Record<string, string> = {
  critical: 'alert-octagon',
  high: 'alert',
  medium: 'alert-circle-outline',
  low: 'information-outline',
};

// === Main App ===
export default function App() {
  const [screen, setScreen] = useState<'dashboard' | 'alerts' | 'setup' | 'thermal' | 'smoke' | 'history' | 'recipes' | 'settings'>('dashboard');
  const [probes, setProbes] = useState<Probe[]>([]);
  const [alerts, setAlerts] = useState<AlertItem[]>([]);
  const [smoke, setSmoke] = useState<SmokeData | null>(null);
  const [grill, setGrill] = useState<GrillData | null>(null);
  const [cookActive, setCookActive] = useState(false);
  const [gasShutoff, setGasShutoff] = useState(false);

  // Simulated data loading
  useEffect(() => {
    const interval = setInterval(() => {
      if (cookActive) {
        setProbes(prev => prev.map(p => ({
          ...p,
          tempTip: Math.min(p.targetTemp, p.tempTip + Math.random() * 0.5),
          tempMid: p.tempTip - 2,
          etaSeconds: Math.max(0, p.etaSeconds - 1),
        })));
        setGrill({
          surfaceMax: 250 + Math.random() * 30,
          surfaceAvg: 180 + Math.random() * 20,
          hotZones: 2 + Math.floor(Math.random() * 3),
          gasPpm: 50 + Math.random() * 30,
          gasLelPct: 0,
          flareupRisk: 15 + Math.random() * 20,
          flareupEtaMs: 0,
        });
      }
    }, 1000);
    return () => clearInterval(interval);
  }, [cookActive]);

  // === Screens ===
  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="dark-content" backgroundColor="#1a1a1a" />
      <Header cookActive={cookActive} gasShutoff={gasShutoff} onNav={setScreen} current={screen} />
      <ScrollView style={styles.content}>
        {screen === 'dashboard' && <DashboardScreen probes={probes} grill={grill} cookActive={cookActive} onStartCook={() => { setCookActive(true); setProbes([createDefaultProbe(0)]); }} />}
        {screen === 'alerts' && <AlertsScreen alerts={alerts} onAck={(id) => setAlerts(prev => prev.map(a => a.id === id ? {...a, acknowledged: true} : a))} />}
        {screen === 'setup' && <SetupScreen onStart={(meat, done) => { setProbes([createProbeWithProfile(0, meat, done)]); setCookActive(true); setScreen('dashboard'); }} />}
        {screen === 'thermal' && <ThermalScreen grill={grill} />}
        {screen === 'smoke' && <SmokeScreen smoke={smoke} />}
        {screen === 'history' && <HistoryScreen />}
        {screen === 'recipes' && <RecipesScreen />}
        {screen === 'settings' && <SettingsScreen gasShutoff={gasShutoff} onGasShutoffToggle={setGasShutoff} />}
      </ScrollView>
      <BottomNav current={screen} onNav={setScreen} />
    </SafeAreaView>
  );
}

function createDefaultProbe(id: number): Probe {
  return { probeId: id, meatType: 0, tempTip: 20, tempMid: 20, tempSurface: 25, tempAmbient: 220, targetTemp: 600, doneness: 0, etaSeconds: 1200, batteryV: 41 };
}

function createProbeWithProfile(id: number, meat: number, done: number): Probe {
  const targets = [[520,540,600,650,710],[0,600,650,700,770],[0,0,0,0,740],[450,550,600,0,0],[520,570,630,0,710]];
  const target = (targets[meat]?.[done - 1] || 600);
  return { probeId: id, meatType: meat, tempTip: 20, tempMid: 20, tempSurface: 25, tempAmbient: 220, targetTemp: target, doneness: 0, etaSeconds: 1800, batteryV: 41 };
}

// === Header ===
function Header({ cookActive, gasShutoff, onNav, current }: any) {
  return (
    <View style={styles.header}>
      <View style={styles.headerLeft}>
        <Icon name="grill" size={28} color="#ff6b35" />
        <Text style={styles.headerTitle}>GrillSync</Text>
      </View>
      <View style={styles.headerRight}>
        {gasShutoff && (
          <View style={[styles.statusBadge, { backgroundColor: '#e74c3c' }]}>
            <Icon name="gas-cylinder" size={14} color="#fff" />
            <Text style={styles.statusText}> GAS OFF</Text>
          </View>
        )}
        {cookActive && (
          <View style={[styles.statusBadge, { backgroundColor: '#27ae60' }]}>
            <Text style={styles.statusText}>COOKING</Text>
          </View>
        )}
      </View>
    </View>
  );
}

// === Dashboard Screen ===
function DashboardScreen({ probes, grill, cookActive, onStartCook }: any) {
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Cook Dashboard</Text>
      {!cookActive ? (
        <View style={styles.emptyState}>
          <Icon name="grill-outline" size={80} color="#555" />
          <Text style={styles.emptyText}>No active cook session</Text>
          <TouchableOpacity style={styles.startButton} onPress={onStartCook}>
            <Icon name="play" size={24} color="#fff" />
            <Text style={styles.startButtonText}>Start Cooking</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <>
          {/* Grill Status Card */}
          {grill && <GrillStatusCard grill={grill} />}
          {/* Probe Cards */}
          {probes.map((probe: Probe) => (
            <ProbeCard key={probe.probeId} probe={probe} />
          ))}
        </>
      )}
    </View>
  );
}

function GrillStatusCard({ grill }: { grill: GrillData }) {
  return (
    <View style={styles.card}>
      <View style={styles.cardHeader}>
        <Icon name="thermometer" size={20} color="#ff6b35" />
        <Text style={styles.cardTitle}>Grill Surface</Text>
      </View>
      <View style={styles.row}>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Max</Text>
          <Text style={[styles.metricValue, { color: grill.surfaceMax > 400 ? '#e74c3c' : '#ff6b35' }]}>
            {(grill.surfaceMax / 10).toFixed(0)}°C
          </Text>
        </View>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Avg</Text>
          <Text style={styles.metricValue}>{(grill.surfaceAvg / 10).toFixed(0)}°C</Text>
        </View>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Hot Zones</Text>
          <Text style={styles.metricValue}>{grill.hotZones}</Text>
        </View>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Gas</Text>
          <Text style={[styles.metricValue, { color: grill.gasPpm > 2100 ? '#e74c3c' : '#27ae60' }]}>
            {grill.gasPpm}ppm
          </Text>
        </View>
      </View>
      {grill.flareupRisk > 50 && (
        <View style={[styles.alertBar, { backgroundColor: '#e67e22' }]}>
          <Icon name="fire" size={16} color="#fff" />
          <Text style={styles.alertBarText}>Flare-up risk: {grill.flareupRisk.toFixed(0)}%</Text>
        </View>
      )}
    </View>
  );
}

function ProbeCard({ probe }: { probe: Probe }) {
  const progress = probe.targetTemp > 0 ? Math.min(1, probe.tempTip / probe.targetTemp) : 0;
  const donenessColor = DONENESS_COLORS[probe.doneness] || '#555';
  return (
    <View style={styles.card}>
      <View style={styles.cardHeader}>
        <Icon name="thermometer-lines" size={20} color={donenessColor} />
        <Text style={styles.cardTitle}>Probe {probe.probeId + 1} — {MEAT_NAMES[probe.meatType]}</Text>
        <View style={[styles.batteryBadge, { backgroundColor: probe.batteryV < 33 ? '#e74c3c' : '#27ae60' }]}>
          <Icon name="battery" size={14} color="#fff" />
          <Text style={styles.batteryText}>{(probe.batteryV / 10).toFixed(1)}V</Text>
        </View>
      </View>
      <View style={styles.row}>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Tip</Text>
          <Text style={[styles.metricValue, { color: donenessColor }]}>
            {(probe.tempTip / 10).toFixed(1)}°C
          </Text>
        </View>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Mid</Text>
          <Text style={styles.metricValue}>{(probe.tempMid / 10).toFixed(1)}°C</Text>
        </View>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>Target</Text>
          <Text style={styles.metricValue}>{(probe.targetTemp / 10).toFixed(1)}°C</Text>
        </View>
        <View style={styles.metric}>
          <Text style={styles.metricLabel}>ETA</Text>
          <Text style={[styles.metricValue, { color: probe.etaSeconds < 120 ? '#e67e22' : '#fff' }]}>
            {Math.floor(probe.etaSeconds / 60)}m{probe.etaSeconds % 60}s
          </Text>
        </View>
      </View>
      {/* Progress bar */}
      <View style={styles.progressTrack}>
        <View style={[styles.progressFill, { width: `${progress * 100}%`, backgroundColor: donenessColor }]} />
      </View>
      <Text style={styles.donenessLabel}>Doneness: {DONENESS_NAMES[probe.doneness]}</Text>
    </View>
  );
}

// === Alerts Screen ===
function AlertsScreen({ alerts, onAck }: any) {
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Alert Center</Text>
      {alerts.length === 0 ? (
        <View style={styles.emptyState}>
          <Icon name="shield-check-outline" size={60} color="#27ae60" />
          <Text style={styles.emptyText}>No active alerts</Text>
          <Text style={styles.emptySubtext}>All systems safe</Text>
        </View>
      ) : (
        <FlatList
          data={alerts.filter((a: AlertItem) => !a.acknowledged)}
          keyExtractor={(item) => item.id}
          renderItem={({ item }) => (
            <AlertCard alert={item} onAck={() => onAck(item.id)} />
          )}
        />
      )}
    </View>
  );
}

function AlertCard({ alert, onAck }: any) {
  const color = SEVERITY_COLORS[alert.severity] || '#3498db';
  const icon = SEVERITY_ICONS[alert.severity] || 'information';
  return (
    <View style={[styles.card, { borderLeftWidth: 4, borderLeftColor: color }]}>
      <View style={styles.cardHeader}>
        <Icon name={icon} size={20} color={color} />
        <Text style={styles.cardTitle}>{alert.alertType.replace(/_/g, ' ')}</Text>
      </View>
      <Text style={styles.alertText}>Severity: {alert.severity.toUpperCase()}</Text>
      <Text style={styles.alertTime}>{alert.timestamp}</Text>
      <TouchableOpacity style={[styles.ackButton, { backgroundColor: color }]} onPress={onAck}>
        <Text style={styles.ackButtonText}>Acknowledge</Text>
      </TouchableOpacity>
    </View>
  );
}

// === Setup Screen ===
function SetupScreen({ onStart }: any) {
  const [meatType, setMeatType] = useState(0);
  const [doneness, setDoneness] = useState(3);
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>New Cook Session</Text>
      <Text style={styles.label}>Select Meat Type</Text>
      <View style={styles.chipRow}>
        {MEAT_NAMES.slice(0, 7).map((meat, i) => (
          <TouchableOpacity
            key={i}
            style={[styles.chip, meatType === i && styles.chipSelected]}
            onPress={() => setMeatType(i)}
          >
            <Text style={[styles.chipText, meatType === i && styles.chipTextSelected]}>{meat}</Text>
          </TouchableOpacity>
        ))}
      </View>
      <Text style={styles.label}>Doneness Level</Text>
      <View style={styles.chipRow}>
        {DONENESS_NAMES.slice(1).map((d, i) => (
          <TouchableOpacity
            key={i}
            style={[styles.chip, { borderColor: DONENESS_COLORS[i + 1] }, doneness === i + 1 && { backgroundColor: DONENESS_COLORS[i + 1] }]}
            onPress={() => setDoneness(i + 1)}
          >
            <Text style={[styles.chipText, doneness === i + 1 && { color: '#fff' }]}>{d}</Text>
          </TouchableOpacity>
        ))}
      </View>
      <TouchableOpacity style={styles.startButton} onPress={() => onStart(meatType, doneness)}>
        <Icon name="fire" size={24} color="#fff" />
        <Text style={styles.startButtonText}>Start Grilling</Text>
      </TouchableOpacity>
    </View>
  );
}

// === Thermal Screen ===
function ThermalScreen({ grill }: any) {
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Thermal Heat Map</Text>
      {grill ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>MLX90640 32×24 Thermal Array</Text>
          <View style={styles.heatMapPlaceholder}>
            {Array.from({ length: 24 }).map((_, row) => (
              <View key={row} style={styles.heatRow}>
                {Array.from({ length: 32 }).map((_, col) => {
                  const temp = 180 + Math.sin(row / 3) * 50 + Math.cos(col / 4) * 30;
                  const hue = temp > 300 ? 0 : temp > 250 ? 20 : temp > 200 ? 40 : 180;
                  return (
                    <View
                      key={col}
                      style={{ width: 8, height: 8, backgroundColor: `hsl(${hue}, 80%, 50%)` }}
                    />
                  );
                })}
              </View>
            ))}
          </View>
          <View style={styles.row}>
            <Text style={styles.metric}>Max: {(grill.surfaceMax / 10).toFixed(0)}°C</Text>
            <Text style={styles.metric}>Avg: {(grill.surfaceAvg / 10).toFixed(0)}°C</Text>
            <Text style={styles.metric}>Zones: {grill.hotZones}</Text>
          </View>
        </View>
      ) : (
        <Text style={styles.emptyText}>No thermal data available</Text>
      )}
    </View>
  );
}

// === Smoke Screen ===
function SmokeScreen({ smoke }: any) {
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Smoke Monitor</Text>
      {!smoke ? (
        <View style={styles.emptyState}>
          <Icon name="smoking-off" size={60} color="#555" />
          <Text style={styles.emptyText}>No smoke node connected</Text>
        </View>
      ) : (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Smoke Quality</Text>
          <Text style={[styles.smokeQuality, { color: smoke.quality === 0 ? '#3498db' : smoke.quality === 2 ? '#e74c3c' : '#fff' }]}>
            {smoke.qualityName}
          </Text>
          <View style={styles.row}>
            <Text>PM2.5: {smoke.pm25.toFixed(1)} µg/m³</Text>
            <Text>VOC: {smoke.vocIndex}</Text>
          </View>
        </View>
      )}
    </View>
  );
}

// === History Screen ===
function HistoryScreen() {
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Cook History</Text>
      <Text style={styles.emptyText}>Previous cook sessions will appear here</Text>
    </View>
  );
}

// === Recipes Screen ===
function RecipesScreen() {
  const recipes = [
    { id: 1, name: 'Reverse Seared Ribeye', meat: 'Beef', temp: '54°C', time: '45 min' },
    { id: 2, name: 'BBQ Pulled Pork', meat: 'Pork', temp: '90°C', time: '8 hours' },
    { id: 3, name: 'Beer Can Chicken', meat: 'Chicken', temp: '74°C', time: '90 min' },
    { id: 4, name: 'Cedar Plank Salmon', meat: 'Fish', temp: '55°C', time: '20 min' },
  ];
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Recipes</Text>
      {recipes.map(r => (
        <View key={r.id} style={styles.card}>
          <Text style={styles.cardTitle}>{r.name}</Text>
          <Text style={styles.recipeInfo}>{r.meat} • Target: {r.temp} • {r.time}</Text>
        </View>
      ))}
    </View>
  );
}

// === Settings Screen ===
function SettingsScreen({ gasShutoff, onGasShutoffToggle }: any) {
  return (
    <View style={styles.screen}>
      <Text style={styles.sectionTitle}>Settings</Text>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Safety</Text>
        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>Auto Gas Shutoff</Text>
          <Switch value={gasShutoff} onValueChange={onGasShutoffToggle} />
        </View>
      </View>
    </View>
  );
}

// === Bottom Navigation ===
function BottomNav({ current, onNav }: any) {
  const tabs = [
    { id: 'dashboard', icon: 'view-dashboard', label: 'Cook' },
    { id: 'alerts', icon: 'bell', label: 'Alerts' },
    { id: 'setup', icon: 'plus-circle', label: 'Setup' },
    { id: 'thermal', icon: 'grid', label: 'Thermal' },
    { id: 'settings', icon: 'cog', label: 'Settings' },
  ];
  return (
    <View style={styles.bottomNav}>
      {tabs.map(tab => (
        <TouchableOpacity
          key={tab.id}
          style={styles.navItem}
          onPress={() => onNav(tab.id)}
        >
          <Icon name={tab.icon} size={24} color={current === tab.id ? '#ff6b35' : '#666'} />
          <Text style={[styles.navLabel, current === tab.id && styles.navLabelActive]}>{tab.label}</Text>
        </TouchableOpacity>
      ))}
    </View>
  );
}

// === Styles ===
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a1a' },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', padding: 16, backgroundColor: '#222', borderBottomWidth: 1, borderBottomColor: '#333' },
  headerLeft: { flexDirection: 'row', alignItems: 'center' },
  headerTitle: { fontSize: 22, fontWeight: 'bold', color: '#ff6b35', marginLeft: 8 },
  headerRight: { flexDirection: 'row', alignItems: 'center' },
  statusBadge: { flexDirection: 'row', alignItems: 'center', paddingHorizontal: 10, paddingVertical: 4, borderRadius: 12, marginLeft: 8 },
  statusText: { color: '#fff', fontSize: 11, fontWeight: '600' },
  content: { flex: 1, padding: 16 },
  screen: { flex: 1 },
  sectionTitle: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  card: { backgroundColor: '#222', borderRadius: 12, padding: 16, marginBottom: 12, borderWidth: 1, borderColor: '#333' },
  cardHeader: { flexDirection: 'row', alignItems: 'center', marginBottom: 12 },
  cardTitle: { fontSize: 16, fontWeight: '600', color: '#fff', marginLeft: 8 },
  row: { flexDirection: 'row', justifyContent: 'space-between' },
  metric: { alignItems: 'center', flex: 1 },
  metricLabel: { fontSize: 12, color: '#888', marginBottom: 4 },
  metricValue: { fontSize: 18, fontWeight: 'bold', color: '#fff' },
  progressTrack: { height: 6, backgroundColor: '#333', borderRadius: 3, marginTop: 12 },
  progressFill: { height: 6, borderRadius: 3 },
  donenessLabel: { fontSize: 12, color: '#888', marginTop: 8, textAlign: 'center' },
  alertBar: { flexDirection: 'row', alignItems: 'center', padding: 8, borderRadius: 8, marginTop: 8 },
  alertBarText: { color: '#fff', fontSize: 13, fontWeight: '600', marginLeft: 8 },
  batteryBadge: { flexDirection: 'row', alignItems: 'center', paddingHorizontal: 6, paddingVertical: 2, borderRadius: 8 },
  batteryText: { color: '#fff', fontSize: 10, fontWeight: '600', marginLeft: 2 },
  emptyState: { alignItems: 'center', justifyContent: 'center', paddingVertical: 60 },
  emptyText: { fontSize: 18, color: '#888', marginTop: 16, textAlign: 'center' },
  emptySubtext: { fontSize: 14, color: '#555', marginTop: 4 },
  startButton: { flexDirection: 'row', alignItems: 'center', justifyContent: 'center', backgroundColor: '#ff6b35', paddingVertical: 14, borderRadius: 12, marginTop: 24 },
  startButtonText: { color: '#fff', fontSize: 18, fontWeight: 'bold', marginLeft: 8 },
  label: { fontSize: 16, color: '#888', marginBottom: 8, marginTop: 16 },
  chipRow: { flexDirection: 'row', flexWrap: 'wrap' },
  chip: { paddingHorizontal: 16, paddingVertical: 8, borderRadius: 20, borderWidth: 1, borderColor: '#444', margin: 4 },
  chipSelected: { backgroundColor: '#ff6b35', borderColor: '#ff6b35' },
  chipText: { color: '#ccc', fontSize: 14 },
  chipTextSelected: { color: '#fff', fontWeight: '600' },
  alertText: { color: '#ccc', fontSize: 14 },
  alertTime: { color: '#666', fontSize: 12, marginTop: 4 },
  ackButton: { paddingVertical: 8, borderRadius: 8, marginTop: 8, alignItems: 'center' },
  ackButtonText: { color: '#fff', fontWeight: '600' },
  heatMapPlaceholder: { flexDirection: 'column', marginVertical: 8, borderRadius: 4, overflow: 'hidden' },
  heatRow: { flexDirection: 'row' },
  smokeQuality: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginVertical: 12 },
  recipeInfo: { color: '#888', fontSize: 14 },
  bottomNav: { flexDirection: 'row', backgroundColor: '#222', borderTopWidth: 1, borderTopColor: '#333', paddingVertical: 8 },
  navItem: { flex: 1, alignItems: 'center', paddingVertical: 4 },
  navLabel: { fontSize: 10, color: '#666', marginTop: 4 },
  navLabelActive: { color: '#ff6b35', fontWeight: '600' },
  settingRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 8 },
  settingLabel: { color: '#ccc', fontSize: 16 },
});