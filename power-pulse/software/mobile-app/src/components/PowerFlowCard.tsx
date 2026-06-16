/**
 * PowerPulse — PowerFlowCard Component
 * 
 * Animated power flow visualization showing energy flowing between
 * solar, battery, grid, and home.
 */

import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import Svg, { Circle, Line, G, Text as SvgText, Defs, LinearGradient, Stop } from 'react-native-svg';
import Animated, { useAnimatedProps, withRepeat, withTiming, useSharedValue } from 'react-native-reanimated';

interface PowerFlowCardProps {
  totalConsumption: number;
  solarProduction: number;
  gridImport: number;
  gridExport: number;
  batteryCharge: number;
  batterySoc: number;
}

const AnimatedCircle = Animated.createAnimatedComponent(Circle);

export function PowerFlowCard({
  totalConsumption,
  solarProduction,
  gridImport,
  gridExport,
  batteryCharge,
  batterySoc,
}: PowerFlowCardProps) {
  const formatWatts = (w: number) => {
    if (w >= 1000) return `${(w / 1000).toFixed(1)} kW`;
    return `${Math.round(w)} W`;
  };

  // Determine power flow direction for animation
  const solarToHome = solarProduction > 0;
  const gridToHome = gridImport > 0;
  const homeToGrid = gridExport > 0;
  const solarToBattery = batteryCharge > 0;

  return (
    <View style={styles.card}>
      <Text style={styles.cardTitle}>Live Power Flow</Text>
      
      <Svg width={320} height={280} viewBox="0 0 320 280">
        <Defs>
          <LinearGradient id="solarGrad" x1="0" y1="0" x2="1" y2="0">
            <Stop offset="0" stopColor="#ffd700" />
            <Stop offset="1" stopColor="#ff8c00" />
          </LinearGradient>
          <LinearGradient id="gridGrad" x1="0" y1="0" x2="1" y2="0">
            <Stop offset="0" stopColor="#4a9eff" />
            <Stop offset="1" stopColor="#0066cc" />
          </LinearGradient>
          <LinearGradient id="homeGrad" x1="0" y1="0" x2="1" y2="0">
            <Stop offset="0" stopColor="#00d4aa" />
            <Stop offset="1" stopColor="#008866" />
          </LinearGradient>
        </Defs>

        {/* Solar icon */}
        <Circle cx={80} cy={40} r={30} fill="#ffd700" opacity={0.2} stroke="#ffd700" strokeWidth={2} />
        <SvgText x={80} y={45} textAnchor="middle" fill="#ffd700" fontSize={12} fontWeight="bold">☀</SvgText>
        <SvgText x={80} y={95} textAnchor="middle" fill="#fff" fontSize={14} fontWeight="bold">
          {formatWatts(solarProduction)}
        </SvgText>

        {/* Grid icon */}
        <Circle cx={240} cy={40} r={30} fill="#4a9eff" opacity={0.2} stroke="#4a9eff" strokeWidth={2} />
        <SvgText x={240} y={45} textAnchor="middle" fill="#4a9eff" fontSize={12} fontWeight="bold">⚡</SvgText>
        <SvgText x={240} y={95} textAnchor="middle" fill="#fff" fontSize={14} fontWeight="bold">
          {formatWatts(gridImport || gridExport)}
        </SvgText>
        <SvgText x={240} y={110} textAnchor="middle" fill="#888" fontSize={10}>
          {gridImport > 0 ? 'importing' : 'exporting'}
        </SvgText>

        {/* Home icon */}
        <Circle cx={160} cy={180} r={40} fill="#00d4aa" opacity={0.2} stroke="#00d4aa" strokeWidth={2} />
        <SvgText x={160} y={185} textAnchor="middle" fill="#00d4aa" fontSize={14} fontWeight="bold">🏠</SvgText>
        <SvgText x={160} y={240} textAnchor="middle" fill="#fff" fontSize={18} fontWeight="bold">
          {formatWatts(totalConsumption)}
        </SvgText>

        {/* Battery icon */}
        <Circle cx={80} cy={220} r={25} fill="#4caf50" opacity={0.2} stroke="#4caf50" strokeWidth={2} />
        <SvgText x={80} y={225} textAnchor="middle" fill="#4caf50" fontSize={10} fontWeight="bold">🔋</SvgText>
        <SvgText x={80} y={260} textAnchor="middle" fill="#fff" fontSize={12} fontWeight="bold">
          {batterySoc}%
        </SvgText>

        {/* Flow lines */}
        {solarToHome && (
          <Line x1={80} y1={70} x2={160} y2={160} stroke="#ffd700" strokeWidth={3} opacity={0.8} />
        )}
        {gridToHome && (
          <Line x1={240} y1={70} x2={160} y2={160} stroke="#4a9eff" strokeWidth={3} opacity={0.8} />
        )}
        {solarToBattery && (
          <Line x1={80} y1={70} x2={80} y2={195} stroke="#4caf50" strokeWidth={2} opacity={0.6} />
        )}
      </Svg>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: '#16213e',
    borderRadius: 12,
    padding: 16,
    marginBottom: 16,
  },
  cardTitle: {
    color: '#fff',
    fontSize: 18,
    fontWeight: '700',
    marginBottom: 8,
  },
});