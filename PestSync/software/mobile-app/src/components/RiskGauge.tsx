/**
 * RiskGauge — Circular infestation risk gauge (0-100%)
 */
import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import Svg, { Circle } from 'react-native-svg';

interface Props {
  risk: number; // 0-1
  size?: number;
}

export default function RiskGauge({ risk, size = 120 }: Props) {
  const radius = size / 2 - 10;
  const circumference = 2 * Math.PI * radius;
  const strokeDashoffset = circumference * (1 - risk);

  const color = risk > 0.7 ? '#e74c3c' : risk > 0.4 ? '#f39c12' : risk > 0.15 ? '#f1c40f' : '#2ecc71';
  const label = risk > 0.7 ? 'CRITICAL' : risk > 0.4 ? 'HIGH' : risk > 0.15 ? 'MODERATE' : 'LOW';

  return (
    <View style={styles.container}>
      <Svg width={size} height={size}>
        <Circle cx={size/2} cy={size/2} r={radius} stroke="#0f3460" strokeWidth="8" fill="none" />
        <Circle
          cx={size/2} cy={size/2} r={radius} stroke={color} strokeWidth="8" fill="none"
          strokeDasharray={circumference}
          strokeDashoffset={strokeDashoffset}
          strokeLinecap="round"
          transform={`rotate(-90 ${size/2} ${size/2})`}
        />
      </Svg>
      <View style={styles.centerText}>
        <Text style={[styles.percent, { color }]}>{Math.round(risk * 100)}%</Text>
        <Text style={[styles.label, { color }]}>{label}</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { alignItems: 'center', justifyContent: 'center', marginVertical: 10 },
  centerText: { position: 'absolute', alignItems: 'center' },
  percent: { fontSize: 28, fontWeight: 'bold' },
  label: { fontSize: 12, fontWeight: 'bold' },
});