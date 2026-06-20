// GreenPulse ScanScreen — trigger leaf scanner, view results

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, FlatList, Image, StyleSheet, TouchableOpacity } from 'react-native';
import { getScans, ScanEntry } from '../api';

export default function ScanScreen({ route }: any) {
  const plantId = route?.params?.plantId ?? 1;
  const [scans, setScans] = useState<ScanEntry[]>([]);

  const load = useCallback(async () => {
    const data = await getScans(plantId);
    setScans(data);
  }, [plantId]);

  useEffect(() => { load(); }, [load]);

  const renderItem = ({ item }: { item: ScanEntry }) => (
    <View style={styles.scanCard}>
      <View style={styles.scanHeader}>
        <Text style={styles.scanDate}>{item.ts.slice(0, 16)}</Text>
        {item.disease !== 'healthy' && (
          <View style={[styles.diseaseBadge,
            { backgroundColor: item.pests > 0 ? '#9C27B0' : '#F44336' }]}>
            <Text style={styles.diseaseBadgeText}>
              {item.pests > 0 ? `Pests: ${item.pests}` : item.disease}
            </Text>
          </View>
        )}
      </View>
      <Text style={styles.scanResult}>
        Result: <Text style={{ fontWeight: '600' }}>{item.disease}</Text>
        {item.disease_conf > 0 && ` (${item.disease_conf}% confidence)`}
      </Text>
      {item.image_url && (
        <Image source={{ uri: item.image_url }} style={styles.scanImage} resizeMode="cover" />
      )}
    </View>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>🔬 Leaf Scans</Text>
      <Text style={styles.subtitle}>Multispectral disease & pest analysis</Text>
      <View style={styles.scannerInfo}>
        <Text style={styles.scannerText}>
          Point the GreenPulse Leaf Scanner at any plant leaf and press Capture.
          It captures 3 shots (white + UV + NIR) in 2 seconds.
        </Text>
      </View>
      <FlatList
        data={scans}
        keyExtractor={(s, i) => i.toString()}
        renderItem={renderItem}
        contentContainerStyle={{ paddingBottom: 20 }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#2E7D32' },
  subtitle: { fontSize: 14, color: '#666', paddingHorizontal: 16, marginBottom: 12 },
  scannerInfo: { backgroundColor: '#E8F5E9', marginHorizontal: 16, padding: 14,
    borderRadius: 10, marginBottom: 16 },
  scannerText: { fontSize: 13, color: '#2E7D32', lineHeight: 20 },
  scanCard: { backgroundColor: '#fff', marginHorizontal: 16, marginBottom: 10,
    borderRadius: 12, padding: 16 },
  scanHeader: { flexDirection: 'row', justifyContent: 'space-between',
    alignItems: 'center', marginBottom: 8 },
  scanDate: { fontSize: 13, color: '#888' },
  diseaseBadge: { paddingHorizontal: 10, paddingVertical: 4, borderRadius: 12 },
  diseaseBadgeText: { fontSize: 12, color: '#fff', fontWeight: '600' },
  scanResult: { fontSize: 15, color: '#333' },
  scanImage: { width: '100%', height: 180, borderRadius: 8, marginTop: 10 },
});