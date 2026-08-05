// SeizureSync — Seizure Diary screen
import React, { useState, useEffect } from 'react';
import { View, Text, FlatList, StyleSheet, TouchableOpacity } from 'react-native';
import axios from 'axios';

const API_BASE = 'https://api.seizuresync.com';

export default function DiaryScreen() {
  const [events, setEvents] = useState<any[]>([]);

  useEffect(() => {
    axios.get(`${API_BASE}/patients/me/events`).then(r => setEvents(r.data));
  }, []);

  const renderItem = ({ item }: { item: any }) => (
    <View style={styles.eventCard}>
      <Text style={styles.date}>{new Date(item.onset).toLocaleString()}</Text>
      <Text style={styles.type}>{item.semiology}</Text>
      <Text style={styles.duration}>Duration: {item.duration_s}s</Text>
      <Text style={styles.confidence}>Confidence: {item.confidence}%</Text>
    </View>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Seizure Diary</Text>
      <FlatList data={events} renderItem={renderItem}
        keyExtractor={(item, i) => i.toString()}
        ListEmptyComponent={<Text style={styles.empty}>No events yet</Text>} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', padding: 16, color: '#0A1144' },
  eventCard: { margin: 8, padding: 16, backgroundColor: '#fff', borderRadius: 8 },
  date: { fontSize: 14, color: '#666' },
  type: { fontSize: 18, fontWeight: 'bold', color: '#0A1144' },
  duration: { fontSize: 14, marginTop: 4 },
  confidence: { fontSize: 12, color: '#999' },
  empty: { textAlign: 'center', marginTop: 40, color: '#999' },
});