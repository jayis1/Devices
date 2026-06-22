// SkinSync LesionTrackerScreen — mole/lesion tracking with change detection

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl, TouchableOpacity } from 'react-native';
import { getLesions, LesionEntry } from '../api';

const STATUS_COLORS: Record<string, string> = {
  stable: '#4CAF50',
  changing: '#FF9800',
  suspect: '#F44336',
};

const LOCATION_NAMES = [
  'Face', 'Left Arm', 'Right Arm', 'Chest', 'Back',
  'Left Leg', 'Right Leg', 'Neck', 'Scalp', 'Hand', 'Foot', 'Abdomen'
];

export default function LesionTrackerScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [lesions, setLesions] = useState<LesionEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const data = await getLesions(userId);
      setLesions(data);
    } catch (e) {
      console.error('Failed to load lesions:', e);
    }
    setRefreshing(false);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  const suspectCount = lesions.filter(l => l.status === 'suspect').length;
  const changingCount = lesions.filter(l => l.status === 'changing').length;

  return (
    <ScrollView style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} />}>
      <Text style={styles.title}>🔍 Lesion Tracker</Text>

      {/* Summary */}
      <View style={styles.summaryRow}>
        <View style={[styles.summaryCard, { backgroundColor: '#4CAF50' }]}>
          <Text style={styles.summaryNum}>{lesions.filter(l => l.status === 'stable').length}</Text>
          <Text style={styles.summaryLabel}>Stable</Text>
        </View>
        <View style={[styles.summaryCard, { backgroundColor: '#FF9800' }]}>
          <Text style={styles.summaryNum}>{changingCount}</Text>
          <Text style={styles.summaryLabel}>Changing</Text>
        </View>
        <View style={[styles.summaryCard, { backgroundColor: '#F44336' }]}>
          <Text style={styles.summaryNum}>{suspectCount}</Text>
          <Text style={styles.summaryLabel}>Suspect</Text>
        </View>
      </View>

      {suspectCount > 0 && (
        <View style={styles.dermAlert}>
          <Text style={styles.dermAlertText}>
            ⚠ {suspectCount} lesion(s) need dermatologist evaluation
          </Text>
          <TouchableOpacity onPress={() => console.log('Export derm report')}>
            <Text style={styles.dermReportLink}>Export Dermatologist Report →</Text>
          </TouchableOpacity>
        </View>
      )}

      {/* Lesion List */}
      <Text style={styles.sectionTitle}>Tracked Lesions</Text>
      {lesions.map((lesion, i) => (
        <View key={i} style={[styles.lesionCard,
          { borderLeftColor: STATUS_COLORS[lesion.status] || '#999' }]}>
          <View style={styles.lesionHeader}>
            <Text style={styles.lesionId}>Lesion #{lesion.lesion_id}</Text>
            <Text style={[styles.lesionStatus, { color: STATUS_COLORS[lesion.status] }]}>
              {lesion.status.toUpperCase()}
            </Text>
          </View>
          <Text style={styles.lesionLocation}>
            Location: {LOCATION_NAMES[lesion.location] || `Area ${lesion.location}`}
          </Text>
          <Text style={styles.lesionDetail}>ABCDE Score: {lesion.abcde}/100</Text>
          <Text style={styles.lesionDetail}>
            First seen: {new Date(lesion.first_seen).toLocaleDateString()}
          </Text>
          <Text style={styles.lesionDetail}>
            Last scanned: {new Date(lesion.last_scanned).toLocaleDateString()}
          </Text>
          {lesion.status === 'suspect' && (
            <Text style={styles.seeDerm}>⚠ See a dermatologist</Text>
          )}
        </View>
      ))}

      {lesions.length === 0 && (
        <Text style={styles.empty}>
          No lesions tracked yet. Scan a mole with the Skin Scanner and mark it to start tracking.
        </Text>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#E91E63' },
  summaryRow: { flexDirection: 'row', paddingHorizontal: 16, marginBottom: 16 },
  summaryCard: { flex: 1, borderRadius: 12, padding: 16, marginHorizontal: 4, alignItems: 'center' },
  summaryNum: { fontSize: 32, fontWeight: 'bold', color: '#fff' },
  summaryLabel: { fontSize: 12, color: '#fff', marginTop: 4 },
  dermAlert: { backgroundColor: '#FFEBEE', padding: 16, marginHorizontal: 16, marginBottom: 16, borderRadius: 10 },
  dermAlertText: { color: '#C62828', fontSize: 16, fontWeight: '600' },
  dermReportLink: { color: '#E91E63', fontSize: 14, marginTop: 8, fontWeight: '500' },
  sectionTitle: { fontSize: 18, fontWeight: 'bold', paddingHorizontal: 16, marginBottom: 8 },
  lesionCard: { backgroundColor: '#fff', padding: 16, marginHorizontal: 16, marginBottom: 8,
    borderRadius: 10, borderLeftWidth: 4 },
  lesionHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  lesionId: { fontSize: 16, fontWeight: 'bold', color: '#333' },
  lesionStatus: { fontSize: 14, fontWeight: 'bold' },
  lesionLocation: { fontSize: 14, color: '#555', marginBottom: 4 },
  lesionDetail: { fontSize: 12, color: '#888', marginTop: 2 },
  seeDerm: { color: '#F44336', fontWeight: '600', marginTop: 8, fontSize: 14 },
  empty: { textAlign: 'center', color: '#999', marginTop: 40, fontSize: 16 },
});