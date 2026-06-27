import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { useStore } from '../store/store';

export default function SettingsScreen() {
  const { device, bleConnect, bleConnected } = useStore();

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Settings</Text>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Device</Text>
        <View style={styles.row}>
          <Text style={styles.label}>Hub ID</Text>
          <Text style={styles.value}>{device?.id || 'Not connected'}</Text>
        </View>
        <View style={styles.row}>
          <Text style={styles.label}>Connection</Text>
          <Text style={[styles.value, { color: bleConnected ? '#4CAF50' : '#F44336' }]}>
            {bleConnected ? 'Connected (BLE)' : 'Disconnected'}
          </Text>
        </View>
        <TouchableOpacity style={styles.button} onPress={bleConnect}>
          <Text style={styles.buttonText}>Reconnect</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Bin Settings</Text>
        <View style={styles.row}>
          <Text style={styles.label}>Volume</Text>
          <Text style={styles.value}>{device?.bin_volume_liters || 200} L</Text>
        </View>
        <View style={styles.row}>
          <Text style={styles.label}>Type</Text>
          <Text style={styles.value}>{device?.compost_type || 'hot'}</Text>
        </View>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Notifications</Text>
        <View style={styles.row}>
          <Text style={styles.label}>Anaerobic alerts</Text>
          <Text style={styles.value}>On</Text>
        </View>
        <View style={styles.row}>
          <Text style={styles.label}>Turn reminders</Text>
          <Text style={styles.value}>Every 7 days</Text>
        </View>
        <View style={styles.row}>
          <Text style={styles.label}>Harvest ready</Text>
          <Text style={styles.value}>On</Text>
        </View>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>About</Text>
        <Text style={styles.about}>CompostSync v1.0.0</Text>
        <Text style={styles.about}>Turning food waste into soil.</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#121212', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  section: { backgroundColor: '#1E1E1E', borderRadius: 12, padding: 16, marginBottom: 12 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', color: '#4CAF50', marginBottom: 8 },
  row: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8, borderBottomWidth: 0.5, borderBottomColor: '#333' },
  label: { color: '#888', fontSize: 14 },
  value: { color: '#fff', fontSize: 14 },
  button: { backgroundColor: '#4CAF50', padding: 12, borderRadius: 8, marginTop: 12, alignItems: 'center' },
  buttonText: { color: '#fff', fontWeight: 'bold' },
  about: { color: '#888', fontSize: 14, marginTop: 4 },
});