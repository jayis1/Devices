/**
 * AllergySync — Settings Screen
 * Allergy profile, node management, medication reminders
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, TextInput, Switch, Alert } from 'react-native';

const API_BASE = 'https://api.allergysync.io/api/v1';

const ALLERGENS = [
  { key: 'birch', label: 'Birch' },
  { key: 'grass', label: 'Grass' },
  { key: 'ragweed', label: 'Ragweed' },
  { key: 'oak', label: 'Oak' },
  { key: 'pine', label: 'Pine' },
  { key: 'mold', label: 'Mold' },
  { key: 'dust_mites', label: 'Dust Mites' },
  { key: 'pet_dander', label: 'Pet Dander' },
];

export default function SettingsScreen() {
  const [sensitivities, setSensitivities] = useState<Record<string, number>>({});
  const [immunotherapy, setImmunotherapy] = useState(false);
  const [nodes, setNodes] = useState<any[]>([]);
  const [serial, setSerial] = useState('');

  const fetchProfile = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/profile`);
      const data = await res.json();
      if (data && data.birch !== undefined) {
        const s: Record<string, number> = {};
        ALLERGENS.forEach(a => { s[a.key] = data[a.key] ?? 0; });
        setSensitivities(s);
        setImmunotherapy(data.immunotherapy ?? false);
      }
    } catch (err) { console.error(err); }
  }, []);

  const fetchNodes = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/nodes`);
      const data = await res.json();
      setNodes(data.nodes || []);
    } catch (err) { console.error(err); }
  }, []);

  useEffect(() => { fetchProfile(); fetchNodes(); }, [fetchProfile, fetchNodes]);

  const saveProfile = async () => {
    try {
      await fetch(`${API_BASE}/profile`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ...sensitivities,
          immunotherapy,
          skin_prick_results: {},
        }),
      });
      Alert.alert('Saved', 'Allergy profile updated.');
    } catch (err) {
      Alert.alert('Error', 'Failed to save profile.');
    }
  };

  const pairNode = async () => {
    if (!serial) return;
    try {
      await fetch(`${API_BASE}/nodes/pair`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ node_type: 'sentinel', serial, pubkey: '' }),
      });
      Alert.alert('Paired', `Node ${serial} paired successfully.`);
      setSerial('');
      fetchNodes();
    } catch (err) {
      Alert.alert('Error', 'Failed to pair node.');
    }
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Allergy Profile</Text>
        <Text style={styles.subtitle}>Rate your sensitivity (0-5) to each allergen</Text>
        {ALLERGENS.map(a => (
          <View key={a.key} style={styles.allergenRow}>
            <Text style={styles.allergenLabel}>{a.label}</Text>
            <View style={styles.sliderRow}>
              {[0, 1, 2, 3, 4, 5].map(n => (
                <TouchableOpacity
                  key={n}
                  onPress={() => setSensitivities(prev => ({ ...prev, [a.key]: n }))}
                  style={[
                    styles.sliderButton,
                    (sensitivities[a.key] ?? 0) === n && styles.sliderButtonActive,
                  ]}
                >
                  <Text style={[
                    styles.sliderText,
                    (sensitivities[a.key] ?? 0) === n && styles.sliderTextActive,
                  ]}>{n}</Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>
        ))}
        <View style={styles.switchRow}>
          <Text style={styles.switchLabel}>On Immunotherapy</Text>
          <Switch value={immunotherapy} onValueChange={setImmunotherapy} />
        </View>
        <TouchableOpacity style={styles.saveButton} onPress={saveProfile}>
          <Text style={styles.saveButtonText}>Save Profile</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.card}>
        <Text style={styles.title}>Paired Nodes</Text>
        {nodes.map((n, i) => (
          <View key={i} style={styles.nodeRow}>
            <Text style={styles.nodeType}>{n.node_type}</Text>
            <Text style={styles.nodeSerial}>{n.serial}</Text>
            <Text style={styles.nodeVersion}>FW {n.firmware_version}</Text>
          </View>
        ))}
        {nodes.length === 0 && <Text style={styles.emptyText}>No nodes paired.</Text>}

        <View style={styles.pairRow}>
          <TextInput
            style={styles.pairInput}
            placeholder="Node serial"
            value={serial}
            onChangeText={setSerial}
          />
          <TouchableOpacity style={styles.pairButton} onPress={pairNode}>
            <Text style={styles.pairButtonText}>Pair</Text>
          </TouchableOpacity>
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { backgroundColor: 'white', margin: 12, padding: 16, borderRadius: 12 },
  title: { fontSize: 18, fontWeight: 'bold', color: '#333' },
  subtitle: { fontSize: 12, color: '#999', marginBottom: 12 },
  allergenRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 6 },
  allergenLabel: { fontSize: 15, color: '#555', width: 100 },
  sliderRow: { flexDirection: 'row', gap: 4 },
  sliderButton: { width: 28, height: 28, borderRadius: 14, borderWidth: 1, borderColor: '#ddd', justifyContent: 'center', alignItems: 'center' },
  sliderButtonActive: { backgroundColor: '#2E7D32', borderColor: '#2E7D32' },
  sliderText: { fontSize: 12, color: '#666' },
  sliderTextActive: { color: 'white', fontWeight: 'bold' },
  switchRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 12 },
  switchLabel: { fontSize: 15, color: '#555' },
  saveButton: { backgroundColor: '#2E7D32', padding: 14, borderRadius: 8, alignItems: 'center', marginTop: 12 },
  saveButtonText: { color: 'white', fontSize: 16, fontWeight: 'bold' },
  nodeRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#f0f0f0' },
  nodeType: { fontSize: 14, color: '#333', fontWeight: '600' },
  nodeSerial: { fontSize: 12, color: '#999' },
  nodeVersion: { fontSize: 12, color: '#999' },
  emptyText: { color: '#999', textAlign: 'center', padding: 20 },
  pairRow: { flexDirection: 'row', marginTop: 12, gap: 8 },
  pairInput: { flex: 1, borderWidth: 1, borderColor: '#ddd', borderRadius: 8, paddingHorizontal: 12, height: 40 },
  pairButton: { backgroundColor: '#2E7D32', paddingHorizontal: 20, justifyContent: 'center', borderRadius: 8 },
  pairButtonText: { color: 'white', fontWeight: 'bold' },
});