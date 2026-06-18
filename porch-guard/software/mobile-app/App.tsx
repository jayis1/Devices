/**
 * App.tsx — PorchGuard Mobile App (React Native)
 *
 * Navigation: Home → PorchStatus, PirateAlert, Deliveries, Lock, Codes, Setup
 *
 * Pirate risk + porch security is the headline feature — risk gauge always
 * visible on the home screen, and a one-tap siren + lock control.
 */

import React, {useState, useEffect} from 'react';
import {NavigationContainer} from '@react-navigation/native';
import {createBottomTabNavigator} from '@react-navigation/bottom-tabs';
import {
  View, Text, StyleSheet, ScrollView, TouchableOpacity,
  SafeAreaView, Alert, RefreshControl, TextInput, Modal, FlatList
} from 'react-native';

const API_BASE = 'http://192.168.1.100:8000';

// ---- Porch Status Screen (Home) ----
function PorchStatusScreen() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);

  const fetchLatest = async () => {
    try {
      const resp = await fetch(`${API_BASE}/api/status`);
      const json = await resp.json();
      setData(json);
    } catch (e) {
      console.error('Fetch failed:', e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchLatest();
    const iv = setInterval(fetchLatest, 3000);
    return () => clearInterval(iv);
  }, []);

  const pirateRisk = data?.pirate_risk || 0;
  const pirateRiskPct = Math.round(pirateRisk * 100);
  const pirateLevel = pirateRisk > 0.95 ? 'EMERGENCY' : pirateRisk > 0.8 ? 'CRITICAL'
    : pirateRisk > 0.6 ? 'WARNING' : 'OK';
  const pirateColor = pirateRisk > 0.8 ? '#f44336' : pirateRisk > 0.6 ? '#ff9800' : '#4caf50';

  const presenceNames = ['Clear', 'Person', 'Parcel only', 'Person + Parcel'];
  const personNames = ['None', 'Resident', 'Courier', 'Unknown', 'Loitering'];
  const parcelNames = ['None', 'Small', 'Medium', 'Large', 'Envelope'];

  const triggerSiren = async () => {
    Alert.alert('Trigger Siren?', 'Sound the 100dB porch siren for 15s?',
      [{text: 'Cancel'}, {text: 'Siren', onPress: () =>
        fetch(`${API_BASE}/api/siren`, {method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({duration_s: 15})})}]);
  };

  const armDisarm = async () => {
    const newArmed = !data?.armed;
    fetch(`${API_BASE}/api/arm`, {method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({armed: newArmed})});
    fetchLatest();
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>PorchGuard</Text>

      {/* Pirate Risk Gauge — always visible */}
      <View style={[styles.pirateGauge, {borderColor: pirateColor}]}>
        <Text style={styles.gaugeLabel}>Pirate Risk</Text>
        <Text style={[styles.gaugeValue, {color: pirateColor}]}>{pirateRiskPct}%</Text>
        <Text style={[styles.gaugeLevel, {color: pirateColor}]}>{pirateLevel}</Text>
        <Text style={styles.gaugeSub}>
          {data?.armed ? '🛡️ Armed' : '⚪ Disarmed'} {data?.siren_active ? ' | 🚨 Siren ON' : ''}
        </Text>
      </View>

      {/* Quick actions */}
      <View style={styles.actionsRow}>
        <TouchableOpacity style={styles.actionBtn} onPress={armDisarm}>
          <Text style={styles.actionBtnText}>{data?.armed ? 'Disarm' : 'Arm'}</Text>
        </TouchableOpacity>
        <TouchableOpacity style={[styles.actionBtn, {backgroundColor: '#c62828'}]}
          onPress={triggerSiren}>
          <Text style={styles.actionBtnText}>🚨 Siren</Text>
        </TouchableOpacity>
      </View>

      <ScrollView refreshControl={<RefreshControl refreshing={loading} onRefresh={fetchLatest} />}>
        {/* Camera status */}
        {data?.camera && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>📷 Porch Camera</Text>
            <View style={styles.row}>
              <Text style={styles.label}>Presence</Text>
              <Text style={styles.value}>{presenceNames[data.camera.presence_state] || 'Clear'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Person</Text>
              <Text style={[styles.value,
                data.camera.person_id >= 3 && styles.warning]}>
                {personNames[data.camera.person_id] || 'None'}
              </Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Parcel</Text>
              <Text style={styles.value}>{parcelNames[data.camera.parcel_class] || 'None'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Distance</Text>
              <Text style={styles.value}>{data.camera.mmwave_dist_cm > 0 ?
                `${data.camera.mmwave_dist_cm}cm` : '—'}</Text>
            </View>
            {data.camera.tamper_flag > 0 && (
              <Text style={styles.critical}>🚨 Camera tamper detected!</Text>
            )}
            {data.camera.power_lost > 0 && (
              <Text style={styles.critical}>⚠️ Camera power lost — possible tamper</Text>
            )}
          </View>
        )}

        {/* Mailbox status */}
        {data?.mailbox && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>📬 Mailbox</Text>
            <View style={styles.row}>
              <Text style={styles.label}>Mail</Text>
              <Text style={styles.value}>
                {['Empty','Letter','Thick','Parcel'][data.mailbox.mail_class] || 'Empty'}
              </Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Weight</Text>
              <Text style={styles.value}>{data.mailbox.weight_mg}mg</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Battery</Text>
              <Text style={styles.value}>{data.mailbox.battery_pct}%</Text>
            </View>
            {data.mailbox.tamper_flag > 0 && (
              <Text style={styles.critical}>🚨 Mailbox tamper!</Text>
            )}
          </View>
        )}

        {/* Lock status */}
        {data?.lock && (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>🔐 Lock</Text>
            <View style={styles.row}>
              <Text style={styles.label}>State</Text>
              <Text style={styles.value}>{data.lock.lock_state ? 'UNLOCKED' : 'LOCKED'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Door</Text>
              <Text style={styles.value}>{data.lock.door_state ? 'OPEN' : 'Closed'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Battery</Text>
              <Text style={styles.value}>{data.lock.battery_pct}%</Text>
            </View>
            {data.lock.door_open_s > 120 && (
              <Text style={styles.warning}>⚠️ Door open {Math.round(data.lock.door_open_s)}s</Text>
            )}
            {data.lock.tamper_flag > 0 && (
              <Text style={styles.critical}>🚨 Lock tamper!</Text>
            )}
          </View>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Pirate Alert Screen ----
function PirateAlertScreen() {
  const [risk, setRisk] = useState(null);
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    const fetchRisk = () => fetch(`${API_BASE}/api/pirate_risk`)
      .then(r => r.json()).then(setRisk).catch(console.error);
    const fetchAlerts = () => fetch(`${API_BASE}/api/alerts?limit=50`)
      .then(r => r.json()).then(setAlerts).catch(console.error);
    fetchRisk(); fetchAlerts();
    const iv = setInterval(() => { fetchRisk(); fetchAlerts(); }, 5000);
    return () => clearInterval(iv);
  }, []);

  const pirateAlerts = alerts.filter(a =>
    a.source === 'camera' && (a.level === 'EMERGENCY' || a.level === 'CRITICAL'));

  const stopSiren = () => fetch(`${API_BASE}/api/siren/off`, {method: 'POST'});

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>🚨 Pirate Watch</Text>
      {risk && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Current Risk: {risk.level}</Text>
          <Text style={styles.bigValue}>{Math.round(risk.risk_score * 100)}%</Text>
          <Text style={styles.label}>Porch: {risk.porch_active ? 'Active' : 'Clear'}</Text>
        </View>
      )}
      <TouchableOpacity style={[styles.actionBtn, {backgroundColor: '#c62828', marginVertical: 12}]}
        onPress={stopSiren}>
        <Text style={styles.actionBtnText}>Silence Siren</Text>
      </TouchableOpacity>
      <ScrollView style={{marginTop: 8}}>
        <Text style={styles.cardTitle}>Recent Pirate Alerts</Text>
        {pirateAlerts.length === 0 && <Text style={styles.label}>No alerts 🎉</Text>}
        {pirateAlerts.map((a, i) => (
          <View key={i} style={[styles.alertCard,
            a.level === 'EMERGENCY' && styles.emergencyCard]}>
            <Text style={styles.alertLevel}>{a.level}</Text>
            <Text style={styles.alertMsg}>{a.message}</Text>
            <Text style={styles.alertTime}>{a.timestamp}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Deliveries Screen ----
function DeliveriesScreen() {
  const [deliveries, setDeliveries] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/api/deliveries?limit=30`)
      .then(r => r.json()).then(setDeliveries).catch(console.error);
  }, []);

  const courierNames = ['Unknown', 'UPS', 'FedEx', 'USPS', 'Amazon', 'DHL', 'Other'];
  const eventNames = ['Parcel drop', 'Mail arrived', 'Mail collected', 'Parcel collected'];
  const parcelNames = ['—', 'Small', 'Medium', 'Large', 'Envelope', 'Letter', 'Thick'];

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>📦 Deliveries</Text>
      <ScrollView>
        {deliveries.length === 0 && <Text style={styles.label}>No deliveries yet</Text>}
        {deliveries.map((d, i) => (
          <View key={i} style={styles.card}>
            <View style={styles.row}>
              <Text style={styles.label}>Event</Text>
              <Text style={styles.value}>{eventNames[d.event_type] || 'Unknown'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Courier</Text>
              <Text style={styles.value}>{courierNames[d.courier_id] || 'Unknown'}</Text>
            </View>
            <View style={styles.row}>
              <Text style={styles.label}>Size</Text>
              <Text style={styles.value}>{parcelNames[d.parcel_class] || '—'}</Text>
            </View>
            {d.has_clip && <Text style={styles.clipLink}>🎬 Clip available (id {d.clip_id})</Text>}
            <Text style={styles.alertTime}>{d.timestamp}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Lock Screen ----
function LockScreen() {
  const [lock, setLock] = useState(null);

  const fetchLock = () => fetch(`${API_BASE}/api/lock/latest`)
    .then(r => r.json()).then(setLock).catch(console.error);

  useEffect(() => {
    fetchLock();
    const iv = setInterval(fetchLock, 3000);
    return () => clearInterval(iv);
  }, []);

  const unlock = () => fetch(`${API_BASE}/api/unlock`, {method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({source: 0, code_id: 0})}).then(fetchLock);

  const lockDoor = () => fetch(`${API_BASE}/api/lock`, {method: 'POST'}).then(fetchLock);

  const openGarage = () => fetch(`${API_BASE}/api/garage`, {method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({duration_s: 1})});

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>🔐 Lock & Garage</Text>
      {lock && (
        <View style={styles.card}>
          <View style={styles.row}>
            <Text style={styles.label}>Lock</Text>
            <Text style={styles.value}>{lock.lock_state ? 'UNLOCKED' : 'LOCKED'}</Text>
          </View>
          <View style={styles.row}>
            <Text style={styles.label}>Door</Text>
            <Text style={styles.value}>{lock.door_state ? 'OPEN' : 'Closed'}</Text>
          </View>
          <View style={styles.row}>
            <Text style={styles.label}>Battery</Text>
            <Text style={styles.value}>{lock.battery_pct}%</Text>
          </View>
        </View>
      )}
      <View style={styles.actionsRow}>
        <TouchableOpacity style={[styles.actionBtn, {backgroundColor: '#2e7d32'}]} onPress={unlock}>
          <Text style={styles.actionBtnText}>Unlock</Text>
        </TouchableOpacity>
        <TouchableOpacity style={[styles.actionBtn, {backgroundColor: '#c62828'}]} onPress={lockDoor}>
          <Text style={styles.actionBtnText}>Lock</Text>
        </TouchableOpacity>
      </View>
      <TouchableOpacity style={[styles.actionBtn, {backgroundColor: '#1565c0', marginTop: 12}]}
        onPress={openGarage}>
        <Text style={styles.actionBtnText}>🚪 Open Garage (1s)</Text>
      </TouchableOpacity>
    </SafeAreaView>
  );
}

// ---- Courier Codes Screen ----
function CodesScreen() {
  const [codes, setCodes] = useState([]);
  const [modalVisible, setModalVisible] = useState(false);
  const [windowMin, setWindowMin] = useState('60');
  const [note, setNote] = useState('');
  const [lastIssued, setLastIssued] = useState(null);

  const fetchCodes = () => fetch(`${API_BASE}/api/codes/active`)
    .then(r => r.json()).then(setCodes).catch(console.error);

  useEffect(() => { fetchCodes(); }, []);

  const issueCode = async () => {
    const resp = await fetch(`${API_BASE}/api/codes/issue`, {method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({window_minutes: parseInt(windowMin) || 60, delivery_note: note})});
    const json = await resp.json();
    setLastIssued(json);
    setModalVisible(false);
    setNote('');
    fetchCodes();
  };

  const revoke = (id) => {
    fetch(`${API_BASE}/api/codes/revoke/${id}`, {method: 'POST'}).then(fetchCodes);
  };

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>🔑 Courier Codes</Text>

      {lastIssued && (
        <View style={[styles.card, {borderColor: '#4caf50', borderWidth: 2}]}>
          <Text style={styles.cardTitle}>✅ New Code Issued</Text>
          <Text style={styles.codeDisplay}>{lastIssued.code}</Text>
          <Text style={styles.label}>Valid for {lastIssued.valid_minutes} minutes</Text>
          <Text style={styles.alertTime}>Share this with the courier. One-time use only.</Text>
        </View>
      )}

      <TouchableOpacity style={styles.actionBtn} onPress={() => setModalVisible(true)}>
        <Text style={styles.actionBtnText}>+ Issue New Code</Text>
      </TouchableOpacity>

      <Text style={[styles.cardTitle, {marginTop: 16}]}>Active Codes ({codes.length})</Text>
      <FlatList
        data={codes}
        keyExtractor={(c) => c.id.toString()}
        renderItem={({item}) => (
          <View style={styles.card}>
            <Text style={styles.codeDisplay}>{item.code_digits}</Text>
            <Text style={styles.label}>Note: {item.note || '—'}</Text>
            <Text style={styles.alertTime}>Issued: {item.issued_at}</Text>
            <TouchableOpacity onPress={() => revoke(item.id)}>
              <Text style={styles.revokeBtn}>Revoke</Text>
            </TouchableOpacity>
          </View>
        )}
      />

      <Modal visible={modalVisible} animationType="slide" transparent={false}>
        <SafeAreaView style={styles.modal}>
          <Text style={styles.title}>Issue Courier Code</Text>
          <Text style={styles.label}>Validity window (minutes):</Text>
          <TextInput style={styles.input} value={windowMin} onChangeText={setWindowMin}
            keyboardType="numeric" />
          <Text style={styles.label}>Note (optional):</Text>
          <TextInput style={styles.input} value={note} onChangeText={setNote}
            placeholder="e.g., Amazon delivery" />
          <View style={styles.actionsRow}>
            <TouchableOpacity style={styles.actionBtn} onPress={issueCode}>
              <Text style={styles.actionBtnText}>Issue</Text>
            </TouchableOpacity>
            <TouchableOpacity style={[styles.actionBtn, {backgroundColor: '#666'}]}
              onPress={() => setModalVisible(false)}>
              <Text style={styles.actionBtnText}>Cancel</Text>
            </TouchableOpacity>
          </View>
        </SafeAreaView>
      </Modal>
    </SafeAreaView>
  );
}

// ---- Setup Wizard ----
function SetupScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.title}>Setup Wizard</Text>
      <ScrollView>
        <Text style={styles.wizardStep}>1. Power on hub node (USB-C)</Text>
        <Text style={styles.wizardStep}>2. Connect via BLE to configure WiFi</Text>
        <Text style={styles.wizardStep}>3. Install porch camera above front door</Text>
        <Text style={styles.wizardStep}>   - Wire to doorbell transformer (16-24VAC)</Text>
        <Text style={styles.wizardStep}>   - Connect to WiFi for clip upload</Text>
        <Text style={styles.wizardStep}>4. Mount mailbox node in/under mailbox</Text>
        <Text style={styles.wizardStep}>   - Solar panel faces up</Text>
        <Text style={styles.wizardStep}>   - Load cell supports mail tray</Text>
        <Text style={styles.wizardStep}>5. Install lock node + garage relay</Text>
        <Text style={styles.wizardStep}>   - Retrofit deadbolt (interior motor)</Text>
        <Text style={styles.wizardStep}>   - Exterior keypad</Text>
        <Text style={styles.wizardStep}>   - Garage relay wires to opener</Text>
        <Text style={styles.wizardStep}>6. Enroll residents (walk past camera ×3 each)</Text>
        <Text style={styles.wizardStep}>7. Run calibration (scripts/calibrate_sensors.py)</Text>
        <Text style={styles.wizardStep}>8. Issue your first courier code!</Text>
        <Text style={styles.wizardStep}>9. Arm the porch — you're protected 🛡️</Text>
      </ScrollView>
    </SafeAreaView>
  );
}

// ---- Navigation ----
const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator>
        <Tab.Screen name="Porch" component={PorchStatusScreen} />
        <Tab.Screen name="Pirate" component={PirateAlertScreen} />
        <Tab.Screen name="Deliveries" component={DeliveriesScreen} />
        <Tab.Screen name="Lock" component={LockScreen} />
        <Tab.Screen name="Codes" component={CodesScreen} />
        <Tab.Screen name="Setup" component={SetupScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: {flex: 1, backgroundColor: '#0a1628', padding: 16},
  title: {fontSize: 24, fontWeight: 'bold', color: '#4fc3f7', marginBottom: 16},
  pirateGauge: {
    backgroundColor: '#1a2a44', borderRadius: 16, padding: 20,
    alignItems: 'center', marginBottom: 12, borderWidth: 2,
  },
  gaugeLabel: {color: '#b0bec5', fontSize: 14, marginBottom: 4},
  gaugeValue: {fontSize: 48, fontWeight: 'bold'},
  gaugeLevel: {fontSize: 16, fontWeight: 'bold', marginTop: 4},
  gaugeSub: {color: '#90a4ae', fontSize: 13, marginTop: 6},
  card: {backgroundColor: '#1a2a44', borderRadius: 12, padding: 16, marginBottom: 12},
  cardTitle: {fontSize: 16, fontWeight: 'bold', color: '#81d4fa', marginBottom: 8},
  row: {flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 6,
    borderBottomWidth: 0.5, borderBottomColor: '#2a3a54'},
  label: {color: '#b0bec5', fontSize: 14, flex: 1},
  value: {fontSize: 16, fontWeight: 'bold', color: '#eceff1', flex: 1, textAlign: 'right'},
  bigValue: {fontSize: 56, fontWeight: 'bold', color: '#eceff1', textAlign: 'center', marginVertical: 8},
  warning: {color: '#ff9800', fontSize: 14, marginTop: 8, fontWeight: 'bold'},
  critical: {color: '#f44336', fontSize: 14, marginTop: 8, fontWeight: 'bold'},
  actionsRow: {flexDirection: 'row', justifyContent: 'space-around', marginBottom: 12},
  actionBtn: {backgroundColor: '#37474f', padding: 14, borderRadius: 8, flex: 1, marginHorizontal: 4, alignItems: 'center'},
  actionBtnText: {color: '#fff', fontWeight: 'bold', fontSize: 15},
  alertCard: {backgroundColor: '#1a2a44', borderRadius: 8, padding: 12, marginBottom: 8},
  emergencyCard: {borderLeftWidth: 4, borderLeftColor: '#f44336'},
  alertLevel: {fontWeight: 'bold', color: '#ff9800', fontSize: 12},
  alertMsg: {color: '#eceff1', fontSize: 14},
  alertTime: {color: '#607d8b', fontSize: 11, marginTop: 4},
  clipLink: {color: '#4fc3f7', fontSize: 13, marginTop: 4},
  codeDisplay: {fontSize: 36, fontWeight: 'bold', color: '#4caf50', textAlign: 'center',
    letterSpacing: 6, marginVertical: 8},
  revokeBtn: {color: '#f44336', fontSize: 14, marginTop: 8, fontWeight: 'bold'},
  wizardStep: {color: '#b0bec5', fontSize: 15, paddingVertical: 10,
    borderBottomWidth: 0.5, borderBottomColor: '#2a3a54'},
  modal: {flex: 1, backgroundColor: '#0a1628', padding: 24},
  input: {backgroundColor: '#1a2a44', color: '#eceff1', fontSize: 18,
    borderRadius: 8, padding: 12, marginVertical: 8},
});