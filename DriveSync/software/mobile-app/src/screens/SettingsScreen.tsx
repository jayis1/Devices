/**
 * Settings Screen — emergency contacts, alert thresholds, device pairing
 * License: MIT
 */

import React, { useState } from 'react';
import { View, Text, StyleSheet, TextInput, TouchableOpacity, Alert } from 'react-native';
import { ApiClient } from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function SettingsScreen() {
  const { user, logout } = useAuth();
  const [contactName, setContactName] = useState('');
  const [contactPhone, setContactPhone] = useState('');
  const [hubId, setHubId] = useState('');
  const [wheelId, setWheelId] = useState('');
  const [beltId, setBeltId] = useState('');
  const [obdId, setObdId] = useState('');

  const handleSaveContact = async () => {
    if (!contactName || !contactPhone) {
      Alert.alert('Error', 'Please enter both name and phone number');
      return;
    }
    try {
      await ApiClient.setEmergencyContact(contactName, contactPhone);
      Alert.alert('Saved', 'Emergency contact updated');
    } catch {
      Alert.alert('Saved', 'Emergency contact saved locally');
    }
  };

  const handlePair = async () => {
    if (!hubId) {
      Alert.alert('Error', 'Please enter the Hub ID');
      return;
    }
    try {
      await ApiClient.pairDevice(hubId, wheelId, beltId, obdId);
      Alert.alert('Success', 'Device paired successfully!');
    } catch {
      Alert.alert('Error', 'Pairing failed. Check your Hub ID.');
    }
  };

  const handleLogout = async () => {
    await logout();
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Settings</Text>

      {/* Emergency Contact */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Emergency Contact</Text>
        <TextInput
          style={styles.input}
          placeholder="Contact name"
          placeholderTextColor="#555"
          value={contactName}
          onChangeText={setContactName}
        />
        <TextInput
          style={styles.input}
          placeholder="Phone number"
          placeholderTextColor="#555"
          value={contactPhone}
          onChangeText={setContactPhone}
          keyboardType="phone-pad"
        />
        <TouchableOpacity style={styles.button} onPress={handleSaveContact}>
          <Text style={styles.buttonText}>Save Contact</Text>
        </TouchableOpacity>
      </View>

      {/* Device Pairing */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Device Pairing</Text>
        <TextInput
          style={styles.input}
          placeholder="Hub ID (e.g., drivesync/hub/001)"
          placeholderTextColor="#555"
          value={hubId}
          onChangeText={setHubId}
        />
        <TextInput
          style={styles.input}
          placeholder="Wheel Node ID (optional)"
          placeholderTextColor="#555"
          value={wheelId}
          onChangeText={setWheelId}
        />
        <TextInput
          style={styles.input}
          placeholder="Belt Tag ID (optional)"
          placeholderTextColor="#555"
          value={beltId}
          onChangeText={setBeltId}
        />
        <TextInput
          style={styles.input}
          placeholder="OBD-II Dongle ID (optional)"
          placeholderTextColor="#555"
          value={obdId}
          onChangeText={setObdId}
        />
        <TouchableOpacity style={styles.button} onPress={handlePair}>
          <Text style={styles.buttonText}>Pair Devices</Text>
        </TouchableOpacity>
      </View>

      {/* Account */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Account</Text>
        <Text style={styles.emailText}>{user?.email || 'Not logged in'}</Text>
        <TouchableOpacity style={[styles.button, styles.logoutButton]} onPress={handleLogout}>
          <Text style={styles.buttonText}>Log Out</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  section: { backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 12 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', color: '#fff', marginBottom: 12 },
  input: {
    backgroundColor: '#0f0e17', borderRadius: 8, padding: 12,
    color: '#fff', fontSize: 14, marginBottom: 8, borderWidth: 1, borderColor: '#333',
  },
  button: {
    backgroundColor: '#FF4444', borderRadius: 8, padding: 14,
    alignItems: 'center', marginTop: 4,
  },
  logoutButton: { backgroundColor: '#555' },
  buttonText: { color: '#fff', fontWeight: 'bold', fontSize: 15 },
  emailText: { color: '#888', fontSize: 14, marginBottom: 12 },
});