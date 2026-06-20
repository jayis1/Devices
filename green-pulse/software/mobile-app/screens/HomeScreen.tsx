// GreenPulse HomeScreen — overview dashboard

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl } from 'react-native';
import { getPlants, Plant, STATUS_NAMES, STATUS_COLORS } from '../api';

export default function HomeScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [plants, setPlants] = useState<Plant[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const data = await getPlants(userId);
      setPlants(data);
    } catch (e) {
      console.error('Failed to load plants:', e);
    }
    setRefreshing(false);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  const needsWater = plants.filter(p => p.status === 1 || p.status === 2);
  const diseased = plants.filter(p => p.status === 4);
  const lowLight = plants.filter(p => p.status === 3);
  const allGood = plants.filter(p => p.status === 0);

  return (
    <ScrollView style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} />}>
      <Text style={styles.title}>🌿 Your Garden</Text>
      <Text style={styles.subtitle}>{plants.length} plants monitored</Text>

      <View style={styles.summaryRow}>
        <View style={[styles.summaryCard, { backgroundColor: '#4CAF50' }]}>
          <Text style={styles.summaryNum}>{allGood.length}</Text>
          <Text style={styles.summaryLabel}>Thriving</Text>
        </View>
        <View style={[styles.summaryCard, { backgroundColor: '#FF9800' }]}>
          <Text style={styles.summaryNum}>{needsWater.length}</Text>
          <Text style={styles.summaryLabel}>Water Soon</Text>
        </View>
        <View style={[styles.summaryCard, { backgroundColor: '#9C27B0' }]}>
          <Text style={styles.summaryNum}>{diseased.length}</Text>
          <Text style={styles.summaryLabel}>Disease</Text>
        </View>
      </View>

      <Text style={styles.sectionTitle}>Needs Attention</Text>
      {[...needsWater, ...diseased, ...lowLight].map(plant => (
        <View key={plant.id} style={styles.plantRow}>
          <View style={[styles.statusDot, { backgroundColor: STATUS_COLORS[plant.status] }]} />
          <View style={{ flex: 1 }}>
            <Text style={styles.plantName}>{plant.name}</Text>
            <Text style={styles.plantSpecies}>{plant.species} • {plant.location}</Text>
          </View>
          <View style={{ alignItems: 'flex-end' }}>
            <Text style={styles.plantStatus}>{STATUS_NAMES[plant.status]}</Text>
            {plant.hours_to_water < 0xFFFF && (
              <Text style={styles.plantDetail}>
                {plant.hours_to_water > 0
                  ? `Water in ${plant.hours_to_water}h`
                  : 'Water now!'}
              </Text>
            )}
            {plant.soil_moisture !== null && (
              <Text style={styles.plantDetail}>Soil: {plant.soil_moisture}%</Text>
            )}
          </View>
        </View>
      ))}

      {plants.length === 0 && (
        <Text style={styles.empty}>No plants yet. Add a Plant Tag to get started!</Text>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#2E7D32' },
  subtitle: { fontSize: 14, color: '#666', paddingHorizontal: 16, marginBottom: 12 },
  summaryRow: { flexDirection: 'row', paddingHorizontal: 16, marginBottom: 16 },
  summaryCard: { flex: 1, borderRadius: 12, padding: 16, marginHorizontal: 4, alignItems: 'center' },
  summaryNum: { fontSize: 32, fontWeight: 'bold', color: '#fff' },
  summaryLabel: { fontSize: 12, color: '#fff', marginTop: 4 },
  sectionTitle: { fontSize: 18, fontWeight: 'bold', paddingHorizontal: 16, marginBottom: 8 },
  plantRow: { flexDirection: 'row', alignItems: 'center', backgroundColor: '#fff',
    padding: 14, marginHorizontal: 16, marginBottom: 8, borderRadius: 10 },
  statusDot: { width: 12, height: 12, borderRadius: 6, marginRight: 12 },
  plantName: { fontSize: 16, fontWeight: '600', color: '#333' },
  plantSpecies: { fontSize: 12, color: '#888' },
  plantStatus: { fontSize: 13, fontWeight: '600', color: '#555' },
  plantDetail: { fontSize: 11, color: '#999' },
  empty: { textAlign: 'center', color: '#999', marginTop: 40, fontSize: 16 },
});