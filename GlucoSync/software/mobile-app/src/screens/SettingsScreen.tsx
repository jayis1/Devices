/**
 * GlucoSync — Settings Screen
 * CGM pairing, pen config, alert thresholds, emergency contacts.
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, TextInput, TouchableOpacity, ScrollView, Alert } from 'react-native';
import { GlucoSyncAPI } from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function SettingsScreen() {
  const { userId, logout } = useAuth();
  const [hypoThreshold, setHypoThreshold] = useState('70');
  const [hyperThreshold, setHyperThreshold] = useState('180');
  const [cgmType, setCgmType] = useState('Dexcom G7');
  const [penType, setPenType] = useState('Bolus (Humalog)');
  const [penUnits, setPenUnits] = useState('2');
  const [contacts, setContacts] = useState<any[]>([]);
  const [contactName, setContactName] = useState('');
  const [contactPhone, setContactPhone] = useState('');

  const fetchContacts = useCallback(async () => {
    if (!userId) return;
    try {
      const data = await GlucoSyncAPI.getEmergencyContacts(userId);
      setContacts(data);
    } catch (e) { console.error(e); }
  }, [userId]);

  useEffect(() => { fetchContacts(); }, [fetchContacts]);

  const addContact = async () => {
    if (!contactName || !contactPhone) return;
    try {
      await GlucoSyncAPI.addEmergencyContact(userId!, contactName, contactPhone, 'Family');
      setContactName('');
      setContactPhone('');
      fetchContacts();
      Alert.alert('Added', 'Emergency contact saved');
    } catch (e) { Alert.alert('Error', 'Failed to add contact'); }
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Settings</Text>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>CGM</Text>
        <Text style={styles.label}>CGM Type</Text>
        <TextInput style={styles.input} value={cgmType} onChangeText={setCgmType} />
        <TouchableOpacity style={styles.pairButton} onPress={() => Alert.alert('Pairing', 'Put CGM in pairing mode and scan')}>
          <Text style={styles.pairButtonText}>Pair CGM</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Insulin Pen Configuration</Text>
        <Text style={styles.label}>Pen Type</Text>
        <TextInput style={styles.input} value={penType} onChangeText={setPenType} />
        <Text style={styles.label}>Units per injection</Text>
        <TextInput style={styles.input} value={penUnits} onChangeText={setPenUnits} keyboardType="numeric" />
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Alert Thresholds</Text>
        <Text style={styles.label}>Hypoglycemia threshold (mg/dL)</Text>
        <TextInput style={styles.input} value={hypoThreshold} onChangeText={setHypoThreshold} keyboardType="numeric" />
        <Text style={styles.label}>Hyperglycemia threshold (mg/dL)</Text>
        <TextInput style={styles.input} value={hyperThreshold} onChangeText={setHyperThreshold} keyboardType="numeric" />
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Emergency Contacts</Text>
        {contacts.map((c, i) => (
          <View key={i} style={styles.contactCard}>
            <Text style={styles.contactName}>{c.name}</Text>
            <Text style={styles.contactPhone}>{c.phone}</Text>
          </View>
        ))}
        <Text style={styles.label}>Add contact</Text>
        <TextInput style={styles.input} placeholder="Name" value={contactName} onChangeText={setContactName} />
        <TextInput style={styles.input} placeholder="Phone" value={contactPhone} onChangeText={setContactPhone} keyboardType="phone-pad" />
        <TouchableOpacity style={styles.addButton} onPress={addContact}>
          <Text style={styles.addButtonText}>+ Add Contact</Text>
        </TouchableOpacity>
      </View>

      <TouchableOpacity style={styles.logoutButton} onPress={logout}>
        <Text style={styles.logoutText}>Log Out</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0F172A', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#F1F5F9', marginBottom: 16 },
  section: { backgroundColor: '#1E293B', borderRadius: 12, padding: 16, marginBottom: 16 },
  sectionTitle: { fontSize: 18, fontWeight: 'bold', color: '#3B82F6', marginBottom: 12 },
  label: { fontSize: 14, color: '#94A3B8', marginTop: 8 },
  input: { backgroundColor: '#0F172A', borderRadius: 8, padding: 12, fontSize: 16, color: '#F1F5F9', marginTop: 4, marginBottom: 8 },
  pairButton: { backgroundColor: '#2563EB', padding: 12, borderRadius: 8, alignItems: 'center', marginTop: 8 },
  pairButtonText: { color: '#FFFFFF', fontWeight: '600' },
  contactCard: { backgroundColor: '#0F172A', borderRadius: 8, padding: 12, marginBottom: 8 },
  contactName: { fontSize: 16, fontWeight: '600', color: '#F1F5F9' },
  contactPhone: { fontSize: 14, color: '#94A3B8' },
  addButton: { backgroundColor: '#10B981', padding: 12, borderRadius: 8, alignItems: 'center', marginTop: 8 },
  addButtonText: { color: '#FFFFFF', fontWeight: '600' },
  logoutButton: { backgroundColor: '#DC2626', padding: 14, borderRadius: 8, alignItems: 'center', marginBottom: 32 },
  logoutText: { color: '#FFFFFF', fontWeight: '600', fontSize: 16 },
});