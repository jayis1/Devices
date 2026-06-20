// GreenPulse WateringScreen — watering log + manual trigger

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, FlatList, StyleSheet, TouchableOpacity, Alert } from 'react-native';
import { getPlants, getWatering, triggerWatering, Plant, WateringEntry } from '../api';

export default function WateringScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [plants, setPlants] = useState<Plant[]>([]);
  const [selectedPlant, setSelectedPlant] = useState<Plant | null>(null);
  const [watering, setWatering] = useState<WateringEntry[]>([]);

  const loadPlants = useCallback(async () => {
    const data = await getPlants(userId);
    setPlants(data);
  }, [userId]);

  const loadWatering = useCallback(async (plantId: number) => {
    const data = await getWatering(plantId);
    setWatering(data);
  }, []);

  useEffect(() => { loadPlants(); }, [loadPlants]);

  const handleWater = async (plant: Plant) => {
    Alert.alert('Water Plant', `Water ${plant.name} now?`,
      [{ text: 'Cancel' },
       { text: 'Water', onPress: async () => {
         const result = await triggerWatering(plant.id);
         Alert.alert('Watering', `Command sent to ${plant.name}. ${result.status}`);
       }}]);
  };

  const statusText = (s: number) =>
    ['OK', 'No Flow (empty?)', 'Leak Detected', 'Timeout (safety)'][s] || 'Unknown';

  const renderItem = ({ item }: { item: WateringEntry }) => (
    <View style={styles.waterRow}>
      <View style={{ flex: 1 }}>
        <Text style={styles.waterDate}>{item.ts.slice(0, 16)}</Text>
        <Text style={styles.waterDetail}>
          {item.ml} ml over {item.duration_s}s • {item.source}
        </Text>
      </View>
      <View style={{ alignItems: 'flex-end' }}>
        <Text style={[styles.waterStatus,
          { color: item.status === 0 ? '#4CAF50' : '#F44336' }]}>
          {statusText(item.status)}
        </Text>
        <Text style={styles.waterMoist}>Pre: {item.pre_moisture}%</Text>
      </View>
    </View>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>💧 Watering</Text>
      <Text style={styles.subtitle}>Manual & automatic watering log</Text>

      <Text style={styles.sectionTitle}>Quick Water</Text>
      <FlatList
        data={plants}
        keyExtractor={(p) => p.id.toString()}
        horizontal
        renderItem={({ item }) => (
          <TouchableOpacity style={styles.quickWaterBtn} onPress={() => handleWater(item)}>
            <Text style={styles.quickWaterName}>{item.name}</Text>
            <Text style={styles.quickWaterSoil}>
              {item.soil_moisture !== null ? `${item.soil_moisture}%` : '--'}
            </Text>
          </TouchableOpacity>
        )}
        contentContainerStyle={{ paddingHorizontal: 16, paddingBottom: 16 }}
      />

      <Text style={styles.sectionTitle}>Watering History</Text>
      <FlatList
        data={watering}
        keyExtractor={(w, i) => i.toString()}
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
  sectionTitle: { fontSize: 16, fontWeight: '600', paddingHorizontal: 16,
    marginBottom: 8, marginTop: 4, color: '#555' },
  quickWaterBtn: { backgroundColor: '#E8F5E9', padding: 14, borderRadius: 12,
    marginRight: 10, alignItems: 'center', minWidth: 80 },
  quickWaterName: { fontSize: 14, fontWeight: '600', color: '#2E7D32' },
  quickWaterSoil: { fontSize: 20, fontWeight: 'bold', color: '#2E7D32', marginTop: 4 },
  waterRow: { flexDirection: 'row', backgroundColor: '#fff', marginHorizontal: 16,
    marginBottom: 8, borderRadius: 10, padding: 14 },
  waterDate: { fontSize: 13, color: '#555', fontWeight: '500' },
  waterDetail: { fontSize: 12, color: '#888', marginTop: 2 },
  waterStatus: { fontSize: 13, fontWeight: '600' },
  waterMoist: { fontSize: 11, color: '#aaa' },
});