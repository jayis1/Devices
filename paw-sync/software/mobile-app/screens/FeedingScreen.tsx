// FeedingScreen — Feeding log + schedule + manual feed button

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity, RefreshControl, Alert } from 'react-native';
import { getFeeding, FeedingEntry } from '../api';

const PET_ID = 1;

export default function FeedingScreen() {
  const [feedings, setFeedings] = useState<FeedingEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchFeeding = useCallback(async () => {
    try {
      const data = await getFeeding(PET_ID);
      setFeedings(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => {
    fetchFeeding();
    const interval = setInterval(fetchFeeding, 60000);
    return () => clearInterval(interval);
  }, [fetchFeeding]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchFeeding();
    setRefreshing(false);
  };

  const handleManualFeed = () => {
    Alert.alert(
      'Manual Feed',
      'Dispense a 50g portion now?',
      [
        { text: 'Cancel', style: 'cancel' },
        { text: 'Feed', onPress: () => console.log('Sending feed command...') },
      ]
    );
  };

  // Calculate daily intake
  const today = new Date().toDateString();
  const todayFeedings = feedings.filter(f => new Date(f.ts).toDateString() === today);
  const todayIntake = todayFeedings.reduce((sum, f) => sum + f.consumed_g, 0);
  const todayTarget = 200; // grams — from weight-loss plan
  const appetiteLossCount = feedings.filter(f => f.appetite_loss).length;

  return (
    <View style={styles.container}>
      <Text style={styles.header}>🍽️ Feeding</Text>

      <View style={styles.statsCard}>
        <View style={styles.statItem}>
          <Text style={styles.statValue}>{todayIntake}g</Text>
          <Text style={styles.statLabel}>Eaten Today</Text>
        </View>
        <View style={styles.statItem}>
          <Text style={styles.statValue}>{todayTarget}g</Text>
          <Text style={styles.statLabel}>Daily Target</Text>
        </View>
        <View style={styles.statItem}>
          <Text style={[styles.statValue, { color: appetiteLossCount > 0 ? '#F44336' : '#4CAF50' }]}>
            {appetiteLossCount}
          </Text>
          <Text style={styles.statLabel}>Appetite Loss</Text>
        </View>
      </View>

      <TouchableOpacity style={styles.feedButton} onPress={handleManualFeed}>
        <Text style={styles.feedButtonText}>🐾 Feed Now (50g)</Text>
      </TouchableOpacity>

      <FlatList
        data={feedings}
        keyExtractor={(_, i) => i.toString()}
        refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
        renderItem={({ item }) => (
          <View style={[styles.feedRow, item.appetite_loss && styles.feedRowAlert]}>
            <View style={styles.feedInfo}>
              <Text style={styles.feedTime}>{new Date(item.ts).toLocaleString()}</Text>
              <Text style={styles.feedAmount}>
                {item.consumed_g}g / {item.dispensed_g}g eaten
              </Text>
              <Text style={styles.feedExtra}>Water: {item.water_ml}ml · Hopper: {item.hopper_pct}%</Text>
            </View>
            {item.appetite_loss && <Text style={styles.alertIcon}>⚠️</Text>}
          </View>
        )}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 22, fontWeight: 'bold', textAlign: 'center', marginVertical: 16 },
  statsCard: { flexDirection: 'row', justifyContent: 'space-around', backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  statItem: { alignItems: 'center' },
  statValue: { fontSize: 24, fontWeight: 'bold', color: '#2196F3' },
  statLabel: { fontSize: 11, color: '#666', marginTop: 4 },
  feedButton: { backgroundColor: '#2196F3', marginHorizontal: 16, padding: 16, borderRadius: 12, alignItems: 'center' },
  feedButtonText: { color: 'white', fontSize: 16, fontWeight: '600' },
  feedRow: { flexDirection: 'row', alignItems: 'center', backgroundColor: 'white', marginHorizontal: 16, marginBottom: 4, padding: 12, borderRadius: 8 },
  feedRowAlert: { borderLeftWidth: 4, borderLeftColor: '#F44336' },
  feedInfo: { flex: 1 },
  feedTime: { fontSize: 12, color: '#999' },
  feedAmount: { fontSize: 14, fontWeight: '500', marginVertical: 2 },
  feedExtra: { fontSize: 11, color: '#666' },
  alertIcon: { fontSize: 20, marginLeft: 8 },
});