/**
 * Trip History Screen — list of past trips with safety scores
 * License: MIT
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity } from 'react-native';
import { ApiClient } from '../api/client';

export default function TripHistoryScreen() {
  const [trips, setTrips] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadTrips();
  }, []);

  const loadTrips = async () => {
    try {
      const data = await ApiClient.getTrips();
      setTrips(data);
    } catch (e) {
      // Demo data
      setTrips([
        { id: '1', start_time: new Date().toISOString(), duration_sec: 2400,
          distance_km: 35.2, safety_score: 85.5, drowsiness_events: 1, distraction_events: 0 },
        { id: '2', start_time: new Date(Date.now() - 86400000).toISOString(),
          duration_sec: 1800, distance_km: 22.8, safety_score: 72.0,
          drowsiness_events: 3, distraction_events: 2 },
      ]);
    }
    setLoading(false);
  };

  const renderItem = ({ item }) => {
    const date = new Date(item.start_time).toLocaleDateString();
    const duration = `${Math.floor(item.duration_sec / 60)}m`;
    const scoreColor = item.safety_score >= 80 ? '#4CAF50' :
                        item.safety_score >= 60 ? '#FF9800' : '#F44336';

    return (
      <TouchableOpacity style={styles.tripCard}>
        <View style={styles.tripHeader}>
          <Text style={styles.tripDate}>{date}</Text>
          <Text style={[styles.tripScore, { color: scoreColor }]}>
            {item.safety_score.toFixed(0)}
          </Text>
        </View>
        <View style={styles.tripDetails}>
          <Text style={styles.tripDetail}>⏱ {duration}</Text>
          <Text style={styles.tripDetail}>📍 {item.distance_km.toFixed(1)} km</Text>
        </View>
        <View style={styles.tripEvents}>
          {item.drowsiness_events > 0 && (
            <Text style={styles.eventBadge}>😴 {item.drowsiness_events} drowsiness</Text>
          )}
          {item.distraction_events > 0 && (
            <Text style={styles.eventBadge}>📱 {item.distraction_events} distraction</Text>
          )}
        </View>
      </TouchableOpacity>
    );
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Trip History</Text>
      <FlatList
        data={trips}
        renderItem={renderItem}
        keyExtractor={(item) => item.id}
        contentContainerStyle={{ paddingBottom: 20 }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  tripCard: {
    backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 12,
  },
  tripHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  tripDate: { fontSize: 16, color: '#fff', fontWeight: '600' },
  tripScore: { fontSize: 28, fontWeight: 'bold' },
  tripDetails: { flexDirection: 'row', gap: 16, marginBottom: 8 },
  tripDetail: { fontSize: 14, color: '#888' },
  tripEvents: { flexDirection: 'row', gap: 8 },
  eventBadge: {
    fontSize: 12, color: '#FF9800', backgroundColor: '#2a1a0e',
    paddingHorizontal: 8, paddingVertical: 4, borderRadius: 4,
  },
});