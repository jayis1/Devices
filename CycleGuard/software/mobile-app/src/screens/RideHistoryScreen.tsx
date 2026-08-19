import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function RideHistoryScreen() {
  const rides = [
    { id: 1, date: '2026-08-18', distance: 12.5, avgSpeed: 18.2, safety: 88 },
    { id: 2, date: '2026-08-17', distance: 8.3, avgSpeed: 15.0, safety: 92 },
    { id: 3, date: '2026-08-16', distance: 22.1, avgSpeed: 22.5, safety: 75 },
  ];
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Ride History</Text>
      {rides.map(r => (
        <View key={r.id} style={styles.rideCard}>
          <Text style={styles.rideDate}>{r.date}</Text>
          <Text style={styles.rideStats}>{r.distance} km · {r.avgSpeed} km/h avg</Text>
          <Text style={styles.rideSafety}>Safety: {r.safety}/100</Text>
        </View>
      ))}
    </ScrollView>
  );
}
const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 20 },
  rideCard: { backgroundColor: '#1a1a1a', padding: 16, borderRadius: 10, marginBottom: 10 },
  rideDate: { fontSize: 16, fontWeight: 'bold', color: '#fff' },
  rideStats: { fontSize: 14, color: '#7f8c8d', marginTop: 4 },
  rideSafety: { fontSize: 14, color: '#2ecc71', marginTop: 4 },
});