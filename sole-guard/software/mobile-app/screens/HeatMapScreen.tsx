/**
 * HeatMapScreen — left/right foot pressure + temperature heat map (SVG)
 */

import React, { useState, useEffect } from 'react';
import { View, StyleSheet, ScrollView } from 'react-native';
import { Text, Card, Title, Paragraph, SegmentedButtons } from 'react-native-paper';
import Svg, { Rect, Text as SvgText } from 'react-native-svg';
import { api } from '../api';

const ZONES = [
  { x: 30, y: 200, w: 40, h: 50, name: 'Heel' },
  { x: 25, y: 130, w: 50, h: 60, name: 'Midfoot' },
  { x: 55, y: 80,  w: 30, h: 50, name: 'M1' },
  { x: 25, y: 80,  w: 30, h: 50, name: 'M2-5' },
  { x: 50, y: 40,  w: 25, h: 40, name: 'Hallux' },
  { x: 25, y: 40,  w: 25, h: 40, name: 'Toes' },
];

function pressureColor(q: number): string {
  if (q < 64)  return '#4caf50';
  if (q < 128) return '#ffeb3b';
  if (q < 192) return '#ff9800';
  return '#f44336';
}

export default function HeatMapScreen() {
  const [data, setData] = useState<any>({ pressure_l: [], pressure_r: [], temp_l: [], temp_r: [] });
  const [mode, setMode] = useState('pressure');
  const patientId = 1;

  useEffect(() => { api.getHeatmap(patientId).then(setData); }, []);

  const renderFoot = (originX: number, pressure: number[], temp: number[], otherTemp: number[], label: string) => {
    return (
      <Svg key={label} width={120} height={270} style={{ marginHorizontal: 20 }}>
        <Rect x={originX} y={20} width={90} height={240} rx={20} fill="#282828" />
        {ZONES.map((z, i) => {
          let fill = '#333';
          if (mode === 'pressure' && pressure.length === 24) {
            const peak = Math.max(...pressure.slice(i*4, (i+1)*4));
            fill = pressureColor(peak);
          } else if (mode === 'temp' && temp.length === 8 && otherTemp.length === 8) {
            const asym = Math.abs(temp[Math.min(i*2,7)] - otherTemp[Math.min(i*2,7)]) / 100;
            fill = asym > 2.2 ? '#f44336' : asym > 1.0 ? '#ff9800' : '#4caf50';
          }
          return <Rect key={i} x={z.x} y={z.y} width={z.w} height={z.h} fill={fill} rx={4} />;
        })}
        <SvgText x={originX + 45} y={15} fill="#fff" fontSize="12" textAnchor="middle">{label}</SvgText>
      </Svg>
    );
  };

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Foot Heat Map</Title>
          <SegmentedButtons
            value={mode}
            onValueChange={setMode}
            buttons={[
              { value: 'pressure', label: 'Pressure' },
              { value: 'temp', label: 'Temp Asymmetry' },
            ]}
          />
          <View style={styles.footRow}>
            {renderFoot(0, data.pressure_l, data.temp_l, data.temp_r, 'Left')}
            {renderFoot(0, data.pressure_r, data.temp_r, data.temp_l, 'Right')}
          </View>
          <Paragraph style={styles.legend}>🟢 Low  🟡 Med  🟠 High  🔴 Critical</Paragraph>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 12 },
  footRow: { flexDirection: 'row', justifyContent: 'center', marginVertical: 16 },
  legend: { textAlign: 'center', marginTop: 8 },
});