// SeizureSync — Settings screen (emergency contacts, escalation, caregiver mode)
import React from 'react';
import { View, Text, TextInput, Switch, StyleSheet, TouchableOpacity } from 'react-native';

export default function SettingsScreen() {
  return (
    <View style={styles.container}>
      <Text style={styles.header}>Settings</Text>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Emergency Contacts</Text>
        <TextInput style={styles.input} placeholder="Primary caregiver phone" />
        <TextInput style={styles.input} placeholder="Secondary contact" />
        <TextInput style={styles.input} placeholder="Emergency address" />
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Alert Escalation</Text>
        <View style={styles.row}>
          <Text>Caregiver alert timeout (s)</Text>
          <TextInput style={styles.smallInput} defaultValue="90" />
        </View>
        <View style={styles.row}>
          <Text>Auto-dispatch 911 (SUDEP)</Text>
          <Switch value={true} />
        </View>
        <View style={styles.row}>
          <Text>{'Auto-dispatch 911 (seizure > 3min)'}</Text>
          <Switch value={false} />
        </View>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Neurologist Reports</Text>
        <TouchableOpacity style={styles.button}>
          <Text style={styles.buttonText}>Share last report</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 16 },
  header: { fontSize: 24, fontWeight: 'bold', color: '#0A1144' },
  section: { marginTop: 20, backgroundColor: '#fff', padding: 16, borderRadius: 8 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8 },
  input: { borderWidth: 1, borderColor: '#ddd', borderRadius: 8, padding: 12, marginBottom: 8 },
  smallInput: { borderWidth: 1, borderColor: '#ddd', borderRadius: 4, padding: 4, width: 60 },
  row: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 8 },
  button: { backgroundColor: '#0A1144', padding: 12, borderRadius: 8, marginTop: 8 },
  buttonText: { color: '#fff', textAlign: 'center' },
});