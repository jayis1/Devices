// InterventionsScreen — Intervention history + efficacy

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, FlatList } from 'react-native';
import { getInterventions, InterventionEntry } from '../api';

const USER_ID = 1;
const INTERVENTION_TYPES = ['Breathing', 'Soundscape', 'Lighting', 'Combined'];

export default function InterventionsScreen() {
  const [interventions, setInterventions] = useState<InterventionEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const data = await getInterventions(USER_ID);
      setInterventions(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  const renderItem = ({ item }: { item: InterventionEntry }) => (
    <View style={styles.card}>
      <View style={styles.cardHeader}>
        <Text style={styles.type}>{INTERVENTION_TYPES[item.type] || 'Unknown'}</Text>
        <Text style={styles.time}>{new Date(item.ts).toLocaleDateString()}</Text>
      </View>
      <Text style={styles.detail}>Duration: {item.duration_s}s</Text>
      <Text style={styles.detail}>Efficacy: {item.efficacy}% (HRV Δ: {item.hrv_delta?.toFixed(1)} ms)</Text>
      <View style={styles.efficacyBar}>
        <View style={[styles.efficacyFill, { width: `${item.efficacy}%`, backgroundColor: item.efficacy > 50 ? '#4CAF50' : '#FF9800' }]} />
      </View>
    </View>
  );

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <Text style={styles.header}>Interventions</Text>
      <Text style={styles.subtitle}>Breathing, soundscapes, lighting — and how much they helped</Text>
      {interventions.length > 0 ? (
        <FlatList data={interventions} renderItem={renderItem} keyExtractor={(item, i) => i.toString()} scrollEnabled={false} />
      ) : <Text style={styles.empty}>No interventions yet</Text>}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  subtitle: { fontSize: 14, color: '#666', textAlign: 'center', marginBottom: 16 },
  card: { backgroundColor: 'white', margin: 16, marginBottom: 8, padding: 16, borderRadius: 12 },
  cardHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  type: { fontSize: 16, fontWeight: 'bold', color: '#6C63FF' },
  time: { fontSize: 12, color: '#999' },
  detail: { fontSize: 13, color: '#333', marginBottom: 4 },
  efficacyBar: { height: 6, backgroundColor: '#eee', borderRadius: 3, marginTop: 8 },
  efficacyFill: { height: 6, borderRadius: 3 },
  empty: { color: '#999', textAlign: 'center', padding: 40 },
});