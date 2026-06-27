import React from 'react';
import { View, StyleSheet } from 'react-native';
import Svg, { Circle, Path, Text as SvgText } from 'react-native-svg';

interface Props {
  score: number;  // 0-100
  size?: number;
}

export default function MaturityGauge({ score, size = 180 }: Props) {
  const radius = size / 2 - 20;
  const circumference = 2 * Math.PI * radius;
  const progress = score / 100;
  const strokeDashoffset = circumference * (1 - progress);

  // Color based on maturity
  const color = score < 30 ? '#FF9800' : score < 70 ? '#FFC107' : '#4CAF50';

  // Phase label
  let phaseLabel = 'Starting';
  if (score >= 90) phaseLabel = 'Cured!';
  else if (score >= 70) phaseLabel = 'Maturing';
  else if (score >= 50) phaseLabel = 'Cooling';
  else if (score >= 20) phaseLabel = 'Cooking';

  return (
    <View style={styles.container}>
      <Svg width={size} height={size}>
        {/* Background circle */}
        <Circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke="#333"
          strokeWidth="12"
        />
        {/* Progress arc */}
        <Circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke={color}
          strokeWidth="12"
          strokeLinecap="round"
          strokeDasharray={circumference}
          strokeDashoffset={strokeDashoffset}
          rotation="-90"
          origin={`${size / 2}, ${size / 2}`}
        />
        {/* Score text */}
        <SvgText
          x={size / 2}
          y={size / 2 - 5}
          textAnchor="middle"
          fontSize="32"
          fontWeight="bold"
          fill={color}
        >
          {Math.round(score)}%
        </SvgText>
        <SvgText
          x={size / 2}
          y={size / 2 + 20}
          textAnchor="middle"
          fontSize="14"
          fill="#888"
        >
          {phaseLabel}
        </SvgText>
      </Svg>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { alignItems: 'center', paddingVertical: 16 },
});