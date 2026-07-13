/**
 * ECGViewerScreen — Real-time ECG waveform + event history
 *
 * License: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, FlatList, ActivityIndicator } from 'react-native';
import { useApi } from '../api/client';

export default function ECGViewerScreen() {
  const api = useApi();
  const [events, setEvents] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchEvents = async () => {
      try {
        const data = await api.getECGEvents(50);
        setEvents(data);
      } catch (e) {
        console.error(e);
      } finally {
        setLoading(false);
      }
    };
    fetchEvents();
  }, []);

  const eventColors = {
    'AFib': '#e74c3c',
    'PVC': '#f39c12',
    'VT': '#c0392b',
    'Bradycardia': '#9b59b6',
  };

  const renderEvent = ({ item }) => (
    <View style={[styles.eventCard, { borderLeftColor: eventColors[item.event_type] || '#3498db' }]}>
      <View style={styles.eventHeader}>
        <Text style={styles.eventType}>{item.event_type}</Text>
        <Text style={styles.eventTime}>
          {new Date(item.timestamp).toLocaleString()}
        </Text>
      </View>
      <Text style={styles.eventDetail}>HR: {item.heart_rate} bpm · Confidence: {(item.confidence * 100).toFixed(0)}%</Text>
    </View>
  );

  if (loading) return <ActivityIndicator size="large" color="#e74c3c" />;

  return (
    <View style={styles.container}>
      <Text style={styles.title}>ECG Events</Text>
      <Text style={styles.subtitle}>{events.length} arrhythmia events detected</Text>
      <FlatList
        data={events}
        renderItem={renderEvent}
        keyExtractor={item => item.id.toString()}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#2c3e50', padding: 20 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff' },
  subtitle: { fontSize: 14, color: '#95a5a6', marginBottom: 15 },
  eventCard: {
    backgroundColor: '#34495e', borderRadius: 8, padding: 15,
    marginBottom: 10, borderLeftWidth: 4,
  },
  eventHeader: { flexDirection: 'row', justifyContent: 'space-between' },
  eventType: { fontSize: 18, fontWeight: 'bold', color: '#fff' },
  eventTime: { fontSize: 12, color: '#95a5a6' },
  eventDetail: { fontSize: 14, color: '#bdc3c7', marginTop: 5 },
});