/**
 * JointSync Mobile App — Dashboard Screen
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { apiClient } from '../api/client';

interface JointSummary {
  id: string;
  type: string;
  side: string;
  current_rom: number;
  current_temp: number;
  flare_risk: number;
}

export default function DashboardScreen() {
  const [joints, setJoints] = useState<JointSummary[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const data = await apiClient.getJoints();
      setJoints(data.map((j: any) => ({
        id: j.id,
        type: j.joint_type,
        side: j.side,
        current_rom: 0,
        current_temp: 0,
        flare_risk: 0,
      })));
    } catch (e) {
      console.error('Failed to fetch joints:', e);
    }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <Text style={styles.title}>Joint Health Overview</Text>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>7-Day Flare Risk</Text>
        <Text style={styles.riskValue}>12%</Text>
        <Text style={styles.riskLabel}>Low Risk</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Connected Tags</Text>
        <Text style={styles.tagCount}>4 / 4</Text>
      </View>

      {joints.map((joint) => (
        <View key={joint.id} style={styles.jointCard}>
          <Text style={styles.jointName}>
            {joint.side} {joint.type}
          </Text>
          <Text style={styles.jointMetric}>ROM: {joint.current_rom.toFixed(0)}°</Text>
          <Text style={styles.jointMetric}>Temp: {joint.current_temp.toFixed(1)}°C</Text>
          <Text style={styles.jointMetric}>Risk: {(joint.flare_risk * 100).toFixed(0)}%</Text>
        </View>
      ))}

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Therapy Adherence</Text>
        <Text style={styles.adherence}>85% this week</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', padding: 20, color: '#333' },
  card: { backgroundColor: '#fff', margin: 10, padding: 20, borderRadius: 12, elevation: 2 },
  cardTitle: { fontSize: 14, color: '#666', marginBottom: 8 },
  riskValue: { fontSize: 48, fontWeight: 'bold', color: '#4CAF50' },
  riskLabel: { fontSize: 16, color: '#666' },
  tagCount: { fontSize: 24, fontWeight: 'bold', color: '#0066CC' },
  jointCard: { backgroundColor: '#fff', margin: 10, padding: 16, borderRadius: 12, elevation: 1 },
  jointName: { fontSize: 18, fontWeight: 'bold', marginBottom: 8, color: '#333' },
  jointMetric: { fontSize: 14, color: '#666', marginBottom: 4 },
  adherence: { fontSize: 24, fontWeight: 'bold', color: '#4CAF50' },
});