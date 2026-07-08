import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet } from 'react-native';
import { Card, Title, Paragraph } from 'react-native-paper';
import { client } from '../api/client';

export default function EventHistoryScreen() {
  const [events, setEvents] = useState<any[]>([]);

  useEffect(() => {
    client.init().then(async () => {
      const evts = await client.getEvents(100);
      setEvents(evts);
    });
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Event History ({events.length})</Title>
          {events.map((event, i) => (
            <View key={i} style={styles.eventRow}>
              <View style={styles.eventLeft}>
                <Text style={styles.eventMagnitude}>M{event.magnitude.toFixed(1)}</Text>
                <Text style={styles.eventSeverity}>
                  {['None', 'Minor', 'Moderate', 'Major', 'Severe'][event.severity]}
                </Text>
              </View>
              <View style={styles.eventRight}>
                <Text style={styles.eventDate}>
                  {new Date(event.timestamp).toLocaleString()}
                </Text>
                <Text style={styles.eventActions}>
                  Gas:{event.actions_taken & 1 ? '✅' : '⬜'}{' '}
                  Water:{event.actions_taken & 2 ? '✅' : '⬜'}{' '}
                  Nodes:{event.node_count}
                </Text>
              </View>
            </View>
          ))}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 8, elevation: 2 },
  eventRow: { flexDirection: 'row', paddingVertical: 10, borderBottomWidth: 0.5, borderBottomColor: '#ddd' },
  eventLeft: { flex: 1 },
  eventMagnitude: { fontSize: 18, fontWeight: 'bold', color: '#e65100' },
  eventSeverity: { fontSize: 12, color: '#666' },
  eventRight: { flex: 2 },
  eventDate: { fontSize: 12 },
  eventActions: { fontSize: 11, color: '#666', marginTop: 4 },
});