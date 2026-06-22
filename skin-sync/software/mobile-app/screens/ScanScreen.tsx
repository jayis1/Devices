// SkinSync ScanScreen — skin scanner results + history

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl, TouchableOpacity } from 'react-native';
import { getScans, ScanEntry } from '../api';

export default function ScanScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [scans, setScans] = useState<ScanEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const data = await getScans(userId);
      setScans(data);
    } catch (e) {
      console.error('Failed to load scans:', e);
    }
    setRefreshing(false);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  return (
    <ScrollView style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} />}>
      <Text style={styles.title}>🔬 Skin Scans</Text>

      <TouchableOpacity style={styles.scanButton} onPress={() => console.log('Trigger scanner')}>
        <Text style={styles.scanButtonText}>📷 New Scan</Text>
      </TouchableOpacity>

      <Text style={styles.sectionTitle}>Recent Scans</Text>
      {scans.map((scan, i) => (
        <View key={i} style={[styles.scanCard,
          scan.abcde > 50 && styles.scanCardAlert]}>
          <View style={styles.scanHeader}>
            <Text style={styles.scanDate}>{new Date(scan.ts).toLocaleDateString()}</Text>
            {scan.abcde > 50 && <Text style={styles.abcdeAlert}>⚠ ABCDE: {scan.abcde}/100</Text>}
          </View>
          <Text style={styles.scanCondition}>
            {scan.condition} ({scan.conf}% confidence)
          </Text>
          {scan.skin_age > 0 && (
            <Text style={styles.scanDetail}>Skin age: {scan.skin_age} years</Text>
          )}
          {scan.lesion_id > 0 && (
            <Text style={styles.scanDetail}>Lesion #{scan.lesion_id}</Text>
          )}
          {scan.abcde > 50 && (
            <Text style={styles.seeDerm}>See a dermatologist</Text>
          )}
        </View>
      ))}

      {scans.length === 0 && (
        <Text style={styles.empty}>No scans yet. Use the Skin Scanner to capture images.</Text>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#E91E63' },
  scanButton: { backgroundColor: '#E91E63', padding: 16, margin: 16, borderRadius: 12,
    alignItems: 'center' },
  scanButtonText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  sectionTitle: { fontSize: 18, fontWeight: 'bold', paddingHorizontal: 16, marginBottom: 8 },
  scanCard: { backgroundColor: '#fff', padding: 16, marginHorizontal: 16, marginBottom: 8,
    borderRadius: 10 },
  scanCardAlert: { borderWidth: 2, borderColor: '#F44336' },
  scanHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  scanDate: { fontSize: 14, color: '#888' },
  abcdeAlert: { fontSize: 14, color: '#F44336', fontWeight: 'bold' },
  scanCondition: { fontSize: 16, fontWeight: '600', color: '#333' },
  scanDetail: { fontSize: 13, color: '#666', marginTop: 4 },
  seeDerm: { color: '#F44336', fontWeight: '600', marginTop: 8 },
  empty: { textAlign: 'center', color: '#999', marginTop: 40, fontSize: 16 },
});