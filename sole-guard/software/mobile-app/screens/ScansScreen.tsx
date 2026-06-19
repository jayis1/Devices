/**
 * ScansScreen — foot-scan image history + wound classifications
 */

import React, { useState, useEffect } from 'react';
import { View, StyleSheet, ScrollView, Image } from 'react-native';
import { Text, Card, Title, Paragraph, List, Chip } from 'react-native-paper';
import { api } from '../api';

const WOUND_COLORS: Record<string, string> = {
  normal: '#4caf50', callus: '#ff9800', blister: '#ffeb3b',
  fissure: '#ff9800', ulcer: '#f44336', fungal: '#9c27b0', maceration: '#2196f3',
};

export default function ScansScreen() {
  const [scans, setScans] = useState<any[]>([]);
  const patientId = 1;

  useEffect(() => { api.getScans(patientId).then(setScans); }, []);

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Foot Scan History</Title>
          <Paragraph>Daily plantar scans with wound detection.</Paragraph>
        </Card.Content>
      </Card>

      {scans.map((s, i) => (
        <Card key={i} style={styles.card}>
          <Card.Content>
            <View style={styles.row}>
              <Text style={styles.foot}>{s.foot === 'L' ? 'Left' : 'Right'} • {s.ts.slice(0, 16)}</Text>
              <Chip style={{ backgroundColor: WOUND_COLORS[s.wound_class] || '#999' }}>
                {s.wound_class} ({s.confidence}%)
              </Chip>
            </View>
            <Paragraph>Weight: {s.weight_kg?.toFixed(1) ?? '-'} kg</Paragraph>
            {s.wound_class !== 'normal' && (
              <Paragraph style={styles.warning}>
                ⚠ {s.wound_class} detected — image sent to your clinician.
              </Paragraph>
            )}
          </Card.Content>
        </Card>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 12 },
  row: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  foot: { fontSize: 16, fontWeight: 'bold' },
  warning: { color: '#f44336', marginTop: 8 },
});