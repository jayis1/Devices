/**
 * RiskGauge — circular gauge component for ulcer-risk score (0-100)
 */

import React from 'react';
import { View, StyleSheet } from 'react-native';
import Svg, { Circle, Text as SvgText } from 'react-native-svg';

export default function RiskGauge({ value, label, color }: { value: number; label: string; color: string }) {
  const radius = 40;
  const circ = 2 * Math.PI * radius;
  const progress = (value / 100) * circ;
  return (
    <View style={styles.container}>
      <Svg width={100} height={100}>
        <Circle cx={50} cy={50} r={radius} stroke="#333" strokeWidth={8} fill="none" />
        <Circle cx={50} cy={50} r={radius} stroke={color} strokeWidth={8} fill="none"
          strokeDasharray={`${progress} ${circ}`} strokeDashoffset={circ / 4}
          strokeLinecap="round" rotation="-90" origin="50,50" />
        <SvgText x={50} y={55} fill={color} fontSize="20" fontWeight="bold" textAnchor="middle">
          {value}
        </SvgText>
      </Svg>
      <SvgText fill="#fff" fontSize="12">{label}</SvgText>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { alignItems: 'center' },
});