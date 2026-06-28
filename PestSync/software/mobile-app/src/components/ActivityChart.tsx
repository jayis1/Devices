/**
 * ActivityChart — Simple bar chart for 24h activity
 */
import React from 'react';
import { View, Text, StyleSheet, Dimensions } from 'react-native';

const { width } = Dimensions.get('window');

interface Props {
  data: number[];
}

export default function ActivityChart({ data }: Props) {
  const max = Math.max(...data, 1);
  const barWidth = (width - 60) / data.length;

  return (
    <View style={styles.container}>
      <View style={styles.chartRow}>
        {data.map((count, i) => {
          const height = (count / max) * 80;
          const color = count === 0 ? '#0f3460' : count < 3 ? '#1a5276' :
                        count < 8 ? '#2980b9' : count < 15 ? '#e67e22' : '#e74c3c';
          return (
            <View key={i} style={styles.barCol}>
              <View style={[styles.bar, { height, backgroundColor: color, width: barWidth - 2 }]} />
              {i % 3 === 0 && <Text style={styles.hourLabel}>{i}h</Text>}
            </View>
          );
        })}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { alignItems: 'center', marginTop: 10 },
  chartRow: { flexDirection: 'row', alignItems: 'flex-end', height: 100, width: '100%' },
  barCol: { alignItems: 'center', justifyContent: 'flex-end' },
  bar: { borderTopLeftRadius: 3, borderTopRightRadius: 3 },
  hourLabel: { color: '#7f8c8d', fontSize: 8, marginTop: 3 },
});