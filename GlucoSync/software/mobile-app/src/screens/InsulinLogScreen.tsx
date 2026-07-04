/**
 * GlucoSync — Insulin Log Screen
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity, Modal, TextInput, Alert } from 'react-native';
import { GlucoSyncAPI, InsulinEntry } from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function InsulinLogScreen() {
  const { userId } = useAuth();
  const [insulin, setInsulin] = useState<InsulinEntry[]>([]);
  const [showManual, setShowManual] = useState(false);
  const [manualUnits, setManualUnits] = useState('');
  const [manualType, setManualType] = useState(1); // 1=bolus

  const fetchInsulin = useCallback(async () => {
    if (!userId) return;
    try {
      const data = await GlucoSyncAPI.getInsulin(userId, 168);
      setInsulin(data);
    } catch (e) { console.error(e); }
  }, [userId]);

  useEffect(() => { fetchInsulin(); }, [fetchInsulin]);

  const addManual = async () => {
    const units = parseInt(manualUnits, 10);
    if (!units || units < 1 || units > 50) {
      Alert.alert('Invalid', 'Enter units between 1 and 50');
      return;
    }
    // TODO: post to API
    setShowManual(false);
    setManualUnits('');
    fetchInsulin();
  };

  const renderItem = ({ item }: { item: InsulinEntry }) => (
    <View style={styles.insulinCard}>
      <View style={styles.insulinHeader}>
        <Text style={styles.insulinType}>{item.pen_type === 0 ? '💊 Basal' : '💉 Bolus'}</Text>
        <Text style={styles.insulinTime}>{new Date(item.created_at).toLocaleString()}</Text>
      </View>
      <View style={styles.insulinStats}>
        <Text style={styles.units}>{item.estimated_units} units</Text>
        {item.confidence > 0 && <Text style={styles.confidence}>Auto-detected ({item.confidence}%)</Text>}
      </View>
    </View>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Insulin Log</Text>
      <TouchableOpacity style={styles.addButton} onPress={() => setShowManual(true)}>
        <Text style={styles.addButtonText}>+ Log Manual Dose</Text>
      </TouchableOpacity>
      <FlatList
        data={insulin}
        keyExtractor={(_, i) => i.toString()}
        renderItem={renderItem}
        ListEmptyComponent={<Text style={styles.empty}>No insulin logged yet</Text>}
      />
      <Modal visible={showManual} animationType="slide" transparent>
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>Log Insulin Dose</Text>
            <TextInput
              style={styles.input}
              placeholder="Units (e.g. 5)"
              keyboardType="numeric"
              value={manualUnits}
              onChangeText={setManualUnits}
            />
            <View style={styles.typeRow}>
              <TouchableOpacity
                style={[styles.typeButton, manualType === 1 && styles.typeButtonActive]}
                onPress={() => setManualType(1)}
              >
                <Text style={manualType === 1 ? styles.typeTextActive : styles.typeText}>Bolus</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={[styles.typeButton, manualType === 0 && styles.typeButtonActive]}
                onPress={() => setManualType(0)}
              >
                <Text style={manualType === 0 ? styles.typeTextActive : styles.typeText}>Basal</Text>
              </TouchableOpacity>
            </View>
            <View style={styles.modalButtons}>
              <TouchableOpacity onPress={() => setShowManual(false)} style={styles.cancelButton}>
                <Text style={styles.cancelText}>Cancel</Text>
              </TouchableOpacity>
              <TouchableOpacity onPress={addManual} style={styles.saveButton}>
                <Text style={styles.saveText}>Save</Text>
              </TouchableOpacity>
            </View>
          </View>
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0F172A', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#F1F5F9', marginBottom: 16 },
  addButton: { backgroundColor: '#2563EB', padding: 14, borderRadius: 8, marginBottom: 16, alignItems: 'center' },
  addButtonText: { color: '#FFFFFF', fontSize: 16, fontWeight: '600' },
  insulinCard: { backgroundColor: '#1E293B', borderRadius: 8, padding: 14, marginBottom: 10 },
  insulinHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  insulinType: { fontSize: 16, fontWeight: '600', color: '#F1F5F9' },
  insulinTime: { fontSize: 12, color: '#94A3B8' },
  insulinStats: { flexDirection: 'row', justifyContent: 'space-between' },
  units: { fontSize: 18, fontWeight: 'bold', color: '#3B82F6' },
  confidence: { fontSize: 12, color: '#10B981' },
  empty: { color: '#64748B', textAlign: 'center', marginTop: 40 },
  modalOverlay: { flex: 1, justifyContent: 'center', backgroundColor: 'rgba(0,0,0,0.5)', padding: 20 },
  modalContent: { backgroundColor: '#1E293B', borderRadius: 12, padding: 20 },
  modalTitle: { fontSize: 18, fontWeight: 'bold', color: '#F1F5F9', marginBottom: 16 },
  input: { backgroundColor: '#0F172A', borderRadius: 8, padding: 12, fontSize: 16, color: '#F1F5F9', marginBottom: 12 },
  typeRow: { flexDirection: 'row', gap: 12, marginBottom: 16 },
  typeButton: { flex: 1, padding: 12, borderRadius: 8, backgroundColor: '#0F172A', alignItems: 'center' },
  typeButtonActive: { backgroundColor: '#2563EB' },
  typeText: { color: '#94A3B8', fontSize: 14 },
  typeTextActive: { color: '#FFFFFF', fontSize: 14, fontWeight: '600' },
  modalButtons: { flexDirection: 'row', gap: 12 },
  cancelButton: { flex: 1, padding: 12, borderRadius: 8, backgroundColor: '#334155', alignItems: 'center' },
  saveButton: { flex: 1, padding: 12, borderRadius: 8, backgroundColor: '#2563EB', alignItems: 'center' },
  cancelText: { color: '#94A3B8', fontSize: 14 },
  saveText: { color: '#FFFFFF', fontSize: 14, fontWeight: '600' },
});