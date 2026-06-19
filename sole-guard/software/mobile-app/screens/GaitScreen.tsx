/**
 * GaitScreen — cadence, stride, symmetry, fall-risk trend
 */

import React, { useState, useEffect } from 'react';
import { View, StyleSheet, ScrollView } from 'react-native';
import { Text, Card, Title, Paragraph } from 'react-native-paper';
import { LineChart } from 'react-native-chart-kit';
import { Dimensions } from 'react-native';
import { api } from '../api';

const { width } = Dimensions.get('window');

export default function GaitScreen() {
  const [gaitData, setGaitData] = useState<any[]>([]);
  const patientId = 1;

  useEffect(() => { api.getGait(patientId).then(setGaitData); }, []);

  const labels = gaitData.slice(-7).map((d) => d.ts.slice(11, 16));
  const symmetryData = gaitData.slice(-7).map((d) => d.gait?.[2] ? d.gait[2] / 10 : 0);

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Gait Symmetry</Title>
          <Paragraph>Lower symmetry = higher fall risk. Trend over recent readings:</Paragraph>
          {symmetryData.length > 0 && (
            <LineChart
              data={{ labels, datasets: [{ data: symmetryData }] }}
              width={width - 60}
              height={180}
              chartConfig={{
                backgroundGradientFrom: '#1a1a2e',
                backgroundGradientTo: '#1a1a2e',
                color: (o) => `rgba(233,30,99,${o})`,
              }}
              bezier
              style={{ marginVertical: 8, borderRadius: 8 }}
            />
          )}
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Current Gait Metrics</Title>
          {gaitData.length > 0 && (
            <>
              <Paragraph>Cadence: {gaitData[0].gait?.[0] ? (gaitData[0].gait[0]/10).toFixed(0) : '-'} spm</Paragraph>
              <Paragraph>Stride: {gaitData[0].gait?.[1] ? gaitData[0].gait[1] : '-'} mm</Paragraph>
              <Paragraph>Symmetry: {gaitData[0].gait?.[2] ? (gaitData[0].gait[2]/10).toFixed(0) : '-'}%</Paragraph>
              <Paragraph>Double support: {gaitData[0].gait?.[3] ? (gaitData[0].gait[3]/100).toFixed(1) : '-'}%</Paragraph>
              <Paragraph>Shuffling score: {gaitData[0].gait?.[4] ? (gaitData[0].gait[4]/10).toFixed(0) : '-'}/100</Paragraph>
              <Paragraph>Steps today: {gaitData[0].gait?.[6] ?? '-'}</Paragraph>
            </>
          )}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 12 },
});