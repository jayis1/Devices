/**
 * HomeScreen — current ulcer-risk score, offload prompt, daily summary
 */

import React, { useState, useEffect } from 'react';
import { View, StyleSheet, ScrollView } from 'react-native';
import { Text, Card, Title, Paragraph, Button, ProgressBar, IconButton } from 'react-native-paper';
import { api } from '../api';
import RiskGauge from '../components/RiskGauge';

export default function HomeScreen({ navigation }: any) {
  const [risk, setRisk] = useState({ risk_l: 0, risk_r: 0, trend: [] });
  const [alerts, setAlerts] = useState<any[]>([]);
  const patientId = 1; // demo

  useEffect(() => {
    api.getRisk(patientId).then(setRisk);
    api.getAlerts(patientId).then(setAlerts);
    const ws = api.connectAlerts(patientId, (a) => {
      setAlerts((prev) => [a, ...prev]);
    });
    return () => ws?.close();
  }, []);

  const maxRisk = Math.max(risk.risk_l, risk.risk_r);
  const riskColor = maxRisk > 65 ? '#f44336' : maxRisk > 40 ? '#ff9800' : '#4caf50';
  const riskLabel = maxRisk > 65 ? 'HIGH — Offload Now' : maxRisk > 40 ? 'Watch' : 'OK';

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Foot Ulcer Risk</Title>
          <View style={styles.gaugeRow}>
            <RiskGauge value={risk.risk_l} label="Left" color={riskColor} />
            <RiskGauge value={risk.risk_r} label="Right" color={riskColor} />
          </View>
          <Text style={[styles.riskLabel, { color: riskColor }]}>{riskLabel}</Text>
          {maxRisk > 65 && (
            <Button mode="contained" color="#f44336" style={styles.offloadBtn}
              onPress={() => api.acknowledgeAlert(patientId, 'offloading')}>
              I'm Offloading Now
            </Button>
          )}
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Latest Alerts</Title>
          {alerts.slice(0, 3).map((a, i) => (
            <Paragraph key={i} style={{ color: a.severity === 'high' ? '#f44336' : '#666' }}>
              ⚠ {a.message}
            </Paragraph>
          ))}
          {alerts.length === 0 && <Paragraph>No alerts — feet look good.</Paragraph>}
          <Button onPress={() => navigation.navigate('Alerts')}>View all</Button>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Quick Actions</Title>
          <View style={styles.actionRow}>
            <Button icon="foot-print" onPress={() => navigation.navigate('Heat Map')}>Heat Map</Button>
            <Button icon="walk" onPress={() => navigation.navigate('Gait')}>Gait</Button>
            <Button icon="camera" onPress={() => navigation.navigate('Scans')}>Scans</Button>
          </View>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 12 },
  gaugeRow: { flexDirection: 'row', justifyContent: 'space-around', marginVertical: 16 },
  riskLabel: { fontSize: 22, fontWeight: 'bold', textAlign: 'center', marginVertical: 8 },
  offloadBtn: { marginTop: 12 },
  actionRow: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 8 },
});