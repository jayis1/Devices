/**
 * GlucoSync — Meal Log Screen
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity, Alert } from 'react-native';
import { GlucoSyncAPI, MealEntry } from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function MealLogScreen() {
  const { userId } = useAuth();
  const [meals, setMeals] = useState<MealEntry[]>([]);

  const fetchMeals = useCallback(async () => {
    if (!userId) return;
    try {
      const data = await GlucoSyncAPI.getMeals(userId, 168); // last 7 days
      setMeals(data);
    } catch (e) { console.error(e); }
  }, [userId]);

  useEffect(() => { fetchMeals(); }, [fetchMeals]);

  const renderItem = ({ item }: { item: MealEntry }) => (
    <View style={styles.mealCard}>
      <View style={styles.mealHeader}>
        <Text style={styles.mealTime}>{new Date(item.created_at).toLocaleString()}</Text>
        <Text style={styles.mealConfidence}>{item.food_confidence}% match</Text>
      </View>
      <View style={styles.mealStats}>
        <Text style={styles.statLabel}>Carbs: <Text style={styles.statValue}>{item.carb_grams}g</Text></Text>
        <Text style={styles.statLabel}>Portion: <Text style={styles.statValue}>{item.portion_grams}g</Text></Text>
        <Text style={styles.statLabel}>GI: <Text style={styles.statValue}>{item.glycemic_index}</Text></Text>
      </View>
    </View>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Meal History</Text>
      <TouchableOpacity
        style={styles.scanButton}
        onPress={() => Alert.alert('Scan', 'Use the GlucoSync Meal Scanner to log a meal')}
      >
        <Text style={styles.scanButtonText}>+ Scan New Meal</Text>
      </TouchableOpacity>
      <FlatList
        data={meals}
        keyExtractor={(_, i) => i.toString()}
        renderItem={renderItem}
        ListEmptyComponent={<Text style={styles.empty}>No meals logged yet</Text>}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0F172A', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#F1F5F9', marginBottom: 16 },
  scanButton: { backgroundColor: '#2563EB', padding: 14, borderRadius: 8, marginBottom: 16, alignItems: 'center' },
  scanButtonText: { color: '#FFFFFF', fontSize: 16, fontWeight: '600' },
  mealCard: { backgroundColor: '#1E293B', borderRadius: 8, padding: 14, marginBottom: 10 },
  mealHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  mealTime: { fontSize: 14, color: '#94A3B8' },
  mealConfidence: { fontSize: 12, color: '#3B82F6' },
  mealStats: { flexDirection: 'row', gap: 16 },
  statLabel: { fontSize: 14, color: '#CBD5E1' },
  statValue: { fontWeight: 'bold', color: '#F1F5F9' },
  empty: { color: '#64748B', textAlign: 'center', marginTop: 40 },
});