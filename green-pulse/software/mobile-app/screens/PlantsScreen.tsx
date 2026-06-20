// GreenPulse PlantsScreen — per-plant detail

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, FlatList, StyleSheet, TouchableOpacity, Modal } from 'react-native';
import { getPlants, getTelemetry, Plant, Telemetry, STATUS_NAMES, STATUS_COLORS } from '../api';

export default function PlantsScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [plants, setPlants] = useState<Plant[]>([]);
  const [selected, setSelected] = useState<Plant | null>(null);
  const [telemetry, setTelemetry] = useState<Telemetry[]>([]);

  const load = useCallback(async () => {
    const data = await getPlants(userId);
    setPlants(data);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  const openDetail = async (plant: Plant) => {
    setSelected(plant);
    const t = await getTelemetry(plant.id);
    setTelemetry(t);
  };

  const renderItem = ({ item }: { item: Plant }) => (
    <TouchableOpacity style={styles.plantCard} onPress={() => openDetail(item)}>
      <View style={[styles.statusDot, { backgroundColor: STATUS_COLORS[item.status] }]} />
      <View style={{ flex: 1 }}>
        <Text style={styles.plantName}>{item.name}</Text>
        <Text style={styles.plantSpecies}>{item.species} • {item.location}</Text>
      </View>
      <View style={{ alignItems: 'flex-end' }}>
        <Text style={styles.moisture}>
          {item.soil_moisture !== null ? `${item.soil_moisture}%` : '--'}
        </Text>
        <Text style={styles.statusText}>{STATUS_NAMES[item.status]}</Text>
      </View>
    </TouchableOpacity>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>🌱 My Plants</Text>
      <FlatList
        data={plants}
        keyExtractor={(p) => p.id.toString()}
        renderItem={renderItem}
        contentContainerStyle={{ paddingBottom: 20 }}
      />
      <Modal visible={!!selected} animationType="slide" onRequestClose={() => setSelected(null)}>
        <View style={styles.modalContent}>
          {selected && (
            <>
              <Text style={styles.modalTitle}>{selected.name}</Text>
              <Text style={styles.modalSpecies}>{selected.species}</Text>
              <Text style={styles.modalLocation}>{selected.location}</Text>
              <Text style={styles.sectionLabel}>Soil Moisture (last 24h)</Text>
              <View style={styles.chartContainer}>
                {telemetry.slice(-24).map((t, i) => (
                  <View key={i} style={[styles.bar, {
                    height: `${Math.max(2, t.soil)}%`,
                    backgroundColor: t.soil < 30 ? '#F44336' : t.soil < 50 ? '#FF9800' : '#4CAF50'
                  }]} />
                ))}
              </View>
              <Text style={styles.sectionLabel}>Recent Readings</Text>
              {telemetry.slice(-5).reverse().map((t, i) => (
                <View key={i} style={styles.readingRow}>
                  <Text style={styles.readingText}>Soil: {t.soil}%</Text>
                  <Text style={styles.readingText}>Temp: {t.temp_c.toFixed(1)}°C</Text>
                  <Text style={styles.readingText}>RH: {t.humidity.toFixed(0)}%</Text>
                </View>
              ))}
              <Text style={styles.sectionLabel}>Battery: {selected.battery_pct ?? '--'}%</Text>
              <TouchableOpacity style={styles.closeBtn} onPress={() => setSelected(null)}>
                <Text style={styles.closeText}>Close</Text>
              </TouchableOpacity>
            </>
          )}
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#2E7D32' },
  plantCard: { flexDirection: 'row', alignItems: 'center', backgroundColor: '#fff',
    padding: 16, marginHorizontal: 16, marginBottom: 8, borderRadius: 12 },
  statusDot: { width: 14, height: 14, borderRadius: 7, marginRight: 14 },
  plantName: { fontSize: 17, fontWeight: '600', color: '#333' },
  plantSpecies: { fontSize: 12, color: '#888' },
  moisture: { fontSize: 22, fontWeight: 'bold', color: '#2E7D32' },
  statusText: { fontSize: 12, color: '#888' },
  modalContent: { flex: 1, backgroundColor: '#FAFAFA', padding: 24 },
  modalTitle: { fontSize: 28, fontWeight: 'bold', color: '#2E7D32' },
  modalSpecies: { fontSize: 16, color: '#666', marginTop: 4 },
  modalLocation: { fontSize: 14, color: '#999', marginTop: 2 },
  sectionLabel: { fontSize: 14, fontWeight: '600', marginTop: 20, marginBottom: 8, color: '#555' },
  chartContainer: { flexDirection: 'row', alignItems: 'flex-end', height: 120,
    backgroundColor: '#fff', borderRadius: 10, padding: 8 },
  bar: { flex: 1, marginHorizontal: 1, borderRadius: 3, minHeight: 2 },
  readingRow: { flexDirection: 'row', justifyContent: 'space-between',
    paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#eee' },
  readingText: { fontSize: 14, color: '#555' },
  closeBtn: { marginTop: 24, backgroundColor: '#2E7D32', padding: 14,
    borderRadius: 10, alignItems: 'center' },
  closeText: { color: '#fff', fontSize: 16, fontWeight: '600' },
});