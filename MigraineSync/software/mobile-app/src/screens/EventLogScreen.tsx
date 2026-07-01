/**
 * MigraineSync — Event Log Screen
 * =================================
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, TouchableOpacity, Modal, TextInput } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { getEvents, EventLog, logEvent } from '../services/api';

export default function EventLogScreen() {
  const [events, setEvents] = useState<EventLog[]>([]);
  const [refreshing, setRefreshing] = useState(false);
  const [showAdd, setShowAdd] = useState(false);
  const [note, setNote] = useState('');

  const fetch = useCallback(async () => {
    try { setEvents(await getEvents(50)); } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetch(); }, [fetch]);

  const onRefresh = async () => { setRefreshing(true); await fetch(); setRefreshing(false); };

  const handleLogSymptom = async (type: string) => {
    await logEvent(type, undefined, note || undefined);
    setNote('');
    setShowAdd(false);
    fetch();
  };

  const severityColors = ['#27AE60', '#F39C12', '#E74C3C', '#C0392B'];
  const eventIcons: Record<string, string> = {
    alert: 'notifications',
    manual: 'create',
    migraine_onset: 'warning',
    medication: 'medkit',
    symptom: 'pulse',
  };

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <View style={styles.header}>
        <Text style={styles.title}>Event Log</Text>
        <TouchableOpacity onPress={() => setShowAdd(true)}>
          <Ionicons name="add-circle" size={32} color="#6C5CE7" />
        </TouchableOpacity>
      </View>

      {events.map((e, i) => (
        <View key={i} style={styles.eventCard}>
          <View style={[styles.severityBar, { backgroundColor: severityColors[e.severity] || '#636E72' }]} />
          <View style={styles.eventContent}>
            <View style={styles.eventHeader}>
              <Ionicons name={(eventIcons[e.event_type] || 'information-circle') as any} size={20} color="#6C5CE7" />
              <Text style={styles.eventType}>{e.event_type.replace(/_/g, ' ')}</Text>
              <Text style={styles.eventTime}>
                {new Date(e.timestamp).toLocaleDateString()} {new Date(e.timestamp).toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'})}
              </Text>
            </View>
            <Text style={styles.eventMessage}>{e.message}</Text>
          </View>
        </View>
      ))}

      {/* Add Event Modal */}
      <Modal visible={showAdd} animationType="slide" transparent={true}>
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>Log Event</Text>
            <TextInput
              style={styles.noteInput}
              placeholder="Add a note (optional)"
              placeholderTextColor="#636E72"
              value={note}
              onChangeText={setNote}
              multiline
            />
            <View style={styles.modalButtons}>
              <TouchableOpacity style={[styles.modalButton, { backgroundColor: '#E74C3C' }]} onPress={() => handleLogSymptom('migraine_onset')}>
                <Text style={styles.modalButtonText}>Migraine</Text>
              </TouchableOpacity>
              <TouchableOpacity style={[styles.modalButton, { backgroundColor: '#3498DB' }]} onPress={() => handleLogSymptom('medication')}>
                <Text style={styles.modalButtonText}>Medication</Text>
              </TouchableOpacity>
              <TouchableOpacity style={[styles.modalButton, { backgroundColor: '#F39C12' }]} onPress={() => handleLogSymptom('symptom')}>
                <Text style={styles.modalButtonText}>Symptom</Text>
              </TouchableOpacity>
            </View>
            <TouchableOpacity onPress={() => setShowAdd(false)}>
              <Text style={styles.cancelText}>Cancel</Text>
            </TouchableOpacity>
          </View>
        </View>
      </Modal>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1A1A2E' },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', padding: 20, paddingTop: 50 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#6C5CE7' },
  eventCard: { flexDirection: 'row', margin: 12, borderRadius: 12, backgroundColor: '#16213E', overflow: 'hidden' },
  severityBar: { width: 4 },
  eventContent: { flex: 1, padding: 12 },
  eventHeader: { flexDirection: 'row', alignItems: 'center' },
  eventType: { fontSize: 12, color: '#A0A0B0', marginLeft: 8, textTransform: 'capitalize', fontWeight: '500' },
  eventTime: { fontSize: 10, color: '#636E72', marginLeft: 'auto' },
  eventMessage: { fontSize: 14, color: '#FFFFFF', marginTop: 8 },
  modalOverlay: { flex: 1, justifyContent: 'center', backgroundColor: 'rgba(0,0,0,0.7)', padding: 20 },
  modalContent: { backgroundColor: '#16213E', borderRadius: 16, padding: 24 },
  modalTitle: { fontSize: 20, fontWeight: 'bold', color: '#6C5CE7', marginBottom: 16 },
  noteInput: { backgroundColor: '#0D1117', borderRadius: 8, padding: 12, color: '#FFFFFF', minHeight: 60, marginBottom: 16 },
  modalButtons: { flexDirection: 'row', justifyContent: 'space-around', marginBottom: 16 },
  modalButton: { padding: 12, borderRadius: 8 },
  modalButtonText: { color: '#FFFFFF', fontSize: 12, fontWeight: '600' },
  cancelText: { textAlign: 'center', color: '#636E72', fontSize: 14 },
});