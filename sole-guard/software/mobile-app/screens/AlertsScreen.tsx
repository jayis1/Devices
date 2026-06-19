/**
 * AlertsScreen — alert history + acknowledgment
 */

import React, { useState, useEffect } from 'react';
import { View, StyleSheet, ScrollView } from 'react-native';
import { Text, Card, Title, Paragraph, Button, List } from 'react-native-paper';
import { api } from '../api';

export default function AlertsScreen() {
  const [alerts, setAlerts] = useState<any[]>([]);
  const patientId = 1;

  useEffect(() => { api.getAlerts(patientId).then(setAlerts); }, []);

  const ack = (id: number) => {
    api.acknowledgeAlert(patientId, id);
    setAlerts((prev) => prev.map((a) => a.id === id ? { ...a, acknowledged: true } : a));
  };

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Alert History</Title>
          {alerts.length === 0 && <Paragraph>No alerts. Feet are healthy.</Paragraph>}
        </Card.Content>
      </Card>

      {alerts.map((a) => (
        <Card key={a.id} style={[
          styles.card,
          a.severity === 'high' && !a.acknowledged && styles.highAlert,
        ]}>
          <Card.Content>
            <View style={styles.row}>
              <Text style={styles.type}>{a.type.toUpperCase()}</Text>
              <Text style={{ color: a.severity === 'high' ? '#f44336' : '#666' }}>{a.severity}</Text>
            </View>
            <Paragraph style={styles.msg}>{a.message}</Paragraph>
            <Text style={styles.ts}>{a.ts}</Text>
            {!a.acknowledged && (
              <Button mode="outlined" onPress={() => ack(a.id)} style={styles.ackBtn}>
                Acknowledge
              </Button>
            )}
            {a.acknowledged && <Text style={styles.acked}>✓ Acknowledged</Text>}
          </Card.Content>
        </Card>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 12 },
  highAlert: { borderLeftWidth: 4, borderLeftColor: '#f44336' },
  row: { flexDirection: 'row', justifyContent: 'space-between' },
  type: { fontWeight: 'bold', fontSize: 14 },
  msg: { marginVertical: 8 },
  ts: { color: '#999', fontSize: 12 },
  ackBtn: { marginTop: 8 },
  acked: { color: '#4caf50', marginTop: 8 },
});