/**
 * SettingsScreen — Emergency contacts, BP schedule, device pairing
 *
 * License: MIT
 */
import React, { useState } from 'react';
import { View, Text, StyleSheet, TextInput, Button, Alert } from 'react-native';
import { useApi } from '../api/client';

export default function SettingsScreen() {
  const api = useApi();
  const [contact1, setContact1] = useState('');
  const [contact2, setContact2] = useState('');

  const saveContacts = async () => {
    try {
      await api.setEmergencyContacts(contact1, contact2);
      Alert.alert('Success', 'Emergency contacts saved');
    } catch (e) {
      Alert.alert('Error', 'Failed to save contacts');
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Settings</Text>

      <Text style={styles.sectionTitle}>Emergency Contacts</Text>
      <TextInput
        style={styles.input}
        placeholder="Primary contact (phone)"
        placeholderTextColor="#7f8c8d"
        value={contact1}
        onChangeText={setContact1}
        keyboardType="phone-pad"
      />
      <TextInput
        style={styles.input}
        placeholder="Secondary contact (phone)"
        placeholderTextColor="#7f8c8d"
        value={contact2}
        onChangeText={setContact2}
        keyboardType="phone-pad"
      />
      <Button title="Save Contacts" onPress={saveContacts} color="#e74c3c" />

      <Text style={styles.sectionTitle}>Devices</Text>
      <View style={styles.deviceItem}>
        <Text style={styles.deviceName}>ECG Chest Patch</Text>
        <Text style={styles.deviceStatus}>● Connected</Text>
      </View>
      <View style={styles.deviceItem}>
        <Text style={styles.deviceName}>Smart Ring</Text>
        <Text style={styles.deviceStatus}>● Connected</Text>
      </View>
      <View style={styles.deviceItem}>
        <Text style={styles.deviceName}>BP Wrist Cuff</Text>
        <Text style={styles.deviceStatus}>● Connected</Text>
      </View>
      <View style={styles.deviceItem}>
        <Text style={styles.deviceName}>CardioSync Hub</Text>
        <Text style={styles.deviceStatus}>● Online</Text>
      </View>

      <Text style={styles.sectionTitle}>About</Text>
      <Text style={styles.about}>CardioSync v1.0.0</Text>
      <Text style={styles.about}>AI-Powered Cardiovascular Health Monitor</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#2c3e50', padding: 20 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 20 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', color: '#e74c3c', marginTop: 20, marginBottom: 10 },
  input: {
    backgroundColor: '#34495e', color: '#fff', borderRadius: 8,
    padding: 12, marginBottom: 10, fontSize: 16,
  },
  deviceItem: {
    flexDirection: 'row', justifyContent: 'space-between',
    backgroundColor: '#34495e', borderRadius: 8, padding: 15, marginBottom: 8,
  },
  deviceName: { fontSize: 16, color: '#fff' },
  deviceStatus: { fontSize: 14, color: '#27ae60' },
  about: { fontSize: 14, color: '#95a5a6', marginTop: 5 },
});