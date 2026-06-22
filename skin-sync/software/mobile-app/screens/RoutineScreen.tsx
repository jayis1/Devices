// SkinSync RoutineScreen — skincare routine + dispensing

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl, TouchableOpacity } from 'react-native';
import { getDispenseHistory, getInventory, DispenseEntry, InventoryItem } from '../api';

export default function RoutineScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [history, setHistory] = useState<DispenseEntry[]>([]);
  const [inventory, setInventory] = useState<InventoryItem[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const [h, inv] = await Promise.all([
        getDispenseHistory(userId),
        getInventory(userId),
      ]);
      setHistory(h);
      setInventory(inv);
    } catch (e) {
      console.error('Failed to load routine data:', e);
    }
    setRefreshing(false);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  return (
    <ScrollView style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} />}>
      <Text style={styles.title}>💧 Skincare Routine</Text>

      {/* Product Inventory */}
      <Text style={styles.sectionTitle}>Products</Text>
      {inventory.map((item, i) => (
        <View key={i} style={styles.productCard}>
          <View style={{ flex: 1 }}>
            <Text style={styles.productName}>Slot {item.slot}: {item.product}</Text>
            <Text style={styles.productRemaining}>{item.remaining_pct}% remaining</Text>
          </View>
          {item.remaining_pct < 15 && (
            <Text style={styles.lowAlert}>Low!</Text>
          )}
        </View>
      ))}

      {/* Today's Routine */}
      <Text style={styles.sectionTitle}>Today's Routine</Text>
      <View style={styles.routineCard}>
        <Text style={styles.routineTime}>🌅 Morning</Text>
        <Text style={styles.routineStep}>1. Cleanser — 1.5ml</Text>
        <Text style={styles.routineStep}>2. Vitamin C Serum — 0.5ml</Text>
        <Text style={styles.routineStep}>3. Moisturizer — 0.8ml</Text>
        <Text style={styles.routineStep}>4. Sunscreen SPF 50 — 1.2ml</Text>
      </View>
      <View style={styles.routineCard}>
        <Text style={styles.routineTime}>🌙 Evening</Text>
        <Text style={styles.routineStep}>1. Cleanser — 1.5ml</Text>
        <Text style={styles.routineStep}>2. Retinol Serum — 0.3ml</Text>
        <Text style={styles.routineStep}>3. Moisturizer — 1.0ml</Text>
      </View>

      {/* Dispensing History */}
      <Text style={styles.sectionTitle}>Recent Dispensing</Text>
      {history.slice(-10).reverse().map((d, i) => (
        <View key={i} style={styles.historyRow}>
          <Text style={styles.historyProduct}>{d.product}</Text>
          <Text style={styles.historyAmount}>{d.mg}mg</Text>
          <Text style={styles.historyTime}>{new Date(d.ts).toLocaleTimeString()}</Text>
        </View>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#E91E63' },
  sectionTitle: { fontSize: 18, fontWeight: 'bold', paddingHorizontal: 16, marginBottom: 8, marginTop: 16 },
  productCard: { flexDirection: 'row', alignItems: 'center', backgroundColor: '#fff',
    padding: 14, marginHorizontal: 16, marginBottom: 8, borderRadius: 10 },
  productName: { fontSize: 16, fontWeight: '600', color: '#333' },
  productRemaining: { fontSize: 12, color: '#888', marginTop: 2 },
  lowAlert: { color: '#FF9800', fontWeight: 'bold', fontSize: 14 },
  routineCard: { backgroundColor: '#fff', padding: 16, marginHorizontal: 16, marginBottom: 8, borderRadius: 10 },
  routineTime: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#E91E63' },
  routineStep: { fontSize: 14, color: '#555', marginBottom: 4 },
  historyRow: { flexDirection: 'row', justifyContent: 'space-between', backgroundColor: '#fff',
    padding: 12, marginHorizontal: 16, marginBottom: 4, borderRadius: 8 },
  historyProduct: { fontSize: 14, fontWeight: '500', color: '#333' },
  historyAmount: { fontSize: 14, color: '#666' },
  historyTime: { fontSize: 12, color: '#999' },
});