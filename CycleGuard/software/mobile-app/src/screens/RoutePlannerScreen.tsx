import React, { useState } from 'react';
import { View, Text, TextInput, Button, StyleSheet, ScrollView } from 'react-native';

export default function RoutePlannerScreen() {
  const [start, setStart] = useState('');
  const [end, setEnd] = useState('');
  const [route, setRoute] = useState<any>(null);

  const planRoute = () => {
    // In production: call POST /api/v1/safety/route
    setRoute({
      avg_safety_score: 85,
      total_distance_km: 5.2,
      estimated_time_min: 18,
      segments: [
        { road_type: 'bike_lane', safety_score: 91 },
        { road_type: 'greenway', safety_score: 95 },
        { road_type: 'residential', safety_score: 72 },
      ],
    });
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Safe Route Planner</Text>
      <Text style={styles.subtitle}>Recommends the safest route, not just shortest</Text>

      <TextInput style={styles.input} placeholder="Start address" value={start}
        onChangeText={setStart} placeholderTextColor="#666" />
      <TextInput style={styles.input} placeholder="Destination" value={end}
        onChangeText={setEnd} placeholderTextColor="#666" />

      <Button title="Find Safe Route" onPress={planRoute} color="#2ecc71" />

      {route && (
        <View style={styles.routeResult}>
          <Text style={styles.routeScore}>Safety: {route.avg_safety_score}/100</Text>
          <Text style={styles.routeDetail}>{route.total_distance_km} km · {route.estimated_time_min} min</Text>

          {route.segments.map((seg: any, i: number) => (
            <View key={i} style={styles.segmentRow}>
              <Text style={styles.segmentType}>{seg.road_type}</Text>
              <Text style={styles.segmentScore}>{seg.safety_score}/100</Text>
            </View>
          ))}
        </View>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 4 },
  subtitle: { fontSize: 14, color: '#7f8c8d', marginBottom: 20 },
  input: { backgroundColor: '#1a1a1a', color: '#fff', padding: 12,
    borderRadius: 8, marginBottom: 10, fontSize: 16 },
  routeResult: { marginTop: 20, backgroundColor: '#1a1a1a',
    padding: 16, borderRadius: 10 },
  routeScore: { fontSize: 22, fontWeight: 'bold', color: '#2ecc71' },
  routeDetail: { fontSize: 14, color: '#7f8c8d', marginTop: 4 },
  segmentRow: { flexDirection: 'row', justifyContent: 'space-between',
    paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#333' },
  segmentType: { fontSize: 14, color: '#fff', textTransform: 'capitalize' },
  segmentScore: { fontSize: 14, color: '#7f8c8d' },
});