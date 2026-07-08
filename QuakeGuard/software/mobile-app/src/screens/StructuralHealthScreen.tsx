import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet } from 'react-native';
import { Card, Title, Paragraph, Button } from 'react-native-paper';
import { client } from '../api/client';

export default function StructuralHealthScreen() {
  const [reports, setReports] = useState<any[]>([]);
  const [summary, setSummary] = useState<any | null>(null);

  useEffect(() => {
    client.init().then(async () => {
      const [rpt, summ] = await Promise.all([
        client.getStructuralReports(50),
        client.getStructuralReport(),
      ]);
      setReports(rpt);
      setSummary(summ);
    });
  }, []);

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Structural Health Summary</Title>
          {summary && (
            <>
              <Paragraph>Report period: {summary.report_period}</Paragraph>
              <Paragraph>Events this period: {summary.total_events}</Paragraph>
              <Paragraph>Max strain: {summary.max_strain} με</Paragraph>
              <Paragraph>Max anomaly score: {summary.max_anomaly}/255</Paragraph>
              <Paragraph>Avg resonance shift: {summary.avg_resonance_shift.toFixed(1)} Hz</Paragraph>
              <Paragraph style={styles.recommendation}>
                {summary.recommendation}
              </Paragraph>
            </>
          )}
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Recent Readings ({reports.length})</Title>
          {reports.slice(0, 20).map((r, i) => (
            <View key={i} style={styles.reportRow}>
              <Text style={styles.reportDate}>
                {new Date(r.timestamp).toLocaleDateString()}
              </Text>
              <Text style={styles.reportStrain}>
                Strain: {r.strain_max_micro} με
              </Text>
              <View style={[
                styles.anomalyBadge,
                { backgroundColor: r.anomaly_score > 128 ? '#f44336' : '#4caf50' }
              ]}>
                <Text style={styles.anomalyText}>{r.anomaly_score}</Text>
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
  recommendation: { marginTop: 12, fontWeight: 'bold', color: '#1565c0' },
  reportRow: { flexDirection: 'row', alignItems: 'center', paddingVertical: 6 },
  reportDate: { flex: 1, fontSize: 12 },
  reportStrain: { flex: 1, fontSize: 12 },
  anomalyBadge: { width: 30, height: 20, borderRadius: 4, justifyContent: 'center', alignItems: 'center' },
  anomalyText: { color: 'white', fontSize: 10, fontWeight: 'bold' },
});