/**
 * TrapsScreen — All trap statuses
 */
import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet, TouchableOpacity, RefreshControl } from 'react-native';
import TrapCard from '../components/TrapCard';
import { fetchTraps, resetTrap } from '../api/client';

export default function TrapsScreen() {
  const [traps, setTraps] = useState<any[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = () => {
    setRefreshing(true);
    fetchTraps()
      .then(setTraps)
      .catch(() => {})
      .finally(() => setRefreshing(false));
  };

  useEffect(() => { load(); }, []);

  const handleReset = (id: string) => {
    resetTrap(id).then(() => load());
  };

  const stats = {
    total: traps.length,
    armed: traps.filter(t => t.status === 'armed').length,
    triggered: traps.filter(t => t.status === 'triggered').length,
    needsAttention: traps.filter(t => t.status === 'triggered' || t.baitLevel < 30).length,
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} tintColor="#e74c3c" />}
    >
      <Text style={styles.title}>Smart Traps</Text>

      {/* Summary */}
      <View style={styles.summaryRow}>
        <View style={styles.summaryBox}>
          <Text style={styles.summaryVal}>{stats.total}</Text>
          <Text style={styles.summaryLabel}>Total</Text>
        </View>
        <View style={styles.summaryBox}>
          <Text style={[styles.summaryVal, { color: '#2ecc71' }]}>{stats.armed}</Text>
          <Text style={styles.summaryLabel}>Armed</Text>
        </View>
        <View style={styles.summaryBox}>
          <Text style={[styles.summaryVal, { color: '#e74c3c' }]}>{stats.triggered}</Text>
          <Text style={styles.summaryLabel}>Triggered</Text>
        </View>
        <View style={styles.summaryBox}>
          <Text style={[styles.summaryVal, { color: '#f39c12' }]}>{stats.needsAttention}</Text>
          <Text style={styles.summaryLabel}>Attention</Text>
        </View>
      </View>

      {/* Trap cards */}
      {traps.map((trap) => (
        <TrapCard key={trap.device_id} trap={trap} onReset={() => handleReset(trap.device_id)} />
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  title: { color: '#fff', fontSize: 22, fontWeight: 'bold', padding: 15 },
  summaryRow: { flexDirection: 'row', justifyContent: 'space-around', marginHorizontal: 10, marginBottom: 10 },
  summaryBox: { alignItems: 'center', backgroundColor: '#16213e', padding: 12, borderRadius: 10, minWidth: 70 },
  summaryVal: { color: '#fff', fontSize: 24, fontWeight: 'bold' },
  summaryLabel: { color: '#95a5a6', fontSize: 11, marginTop: 2 },
});