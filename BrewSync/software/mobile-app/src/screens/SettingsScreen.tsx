import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function SettingsScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>⚙ Settings</Text>
      <View style={styles.card}>
        <Text style={styles.label}>Hub Connection</Text>
        <Text style={styles.value}>brewsync-hub.local:8080</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.label}>Account</Text>
        <Text style={styles.value}>brewer@example.com</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.label}>Notifications</Text>
        <Text style={styles.value}>Temperature alerts: ON</Text>
        <Text style={styles.value}>Stuck fermentation: ON</Text>
        <Text style={styles.value}>Target FG reached: ON</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.label}>Temperature Units</Text>
        <Text style={styles.value}>°C</Text>
      </View>
      <View style={styles.card}>
        <Text style={styles.label}>Firmware</Text>
        <Text style={styles.value}>Hub: v1.2.0 | Fermenter: v1.2.0</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  card: { backgroundColor: '#161b22', borderRadius: 12, padding: 16, marginBottom: 12 },
  label: { fontSize: 14, color: '#8b949e', marginBottom: 4 },
  value: { fontSize: 16, color: '#e6e6e6' },
});