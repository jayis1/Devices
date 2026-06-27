import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface Props {
  icon: string;
  label: string;
  value: string;
  status: 'good' | 'warning' | 'bad';
}

export default function SensorCard({ icon, label, value, status }: Props) {
  const borderColor = status === 'good' ? '#4CAF50' : status === 'warning' ? '#FFC107' : '#F44336';

  return (
    <View style={[styles.card, { borderLeftColor: borderColor }]}>
      <Text style={styles.icon}>{icon}</Text>
      <Text style={styles.label}>{label}</Text>
      <Text style={styles.value}>{value}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    flex: 1,
    margin: 4,
    padding: 12,
    backgroundColor: '#1E1E1E',
    borderRadius: 12,
    borderLeftWidth: 3,
  },
  icon: { fontSize: 24 },
  label: { fontSize: 11, color: '#888', marginTop: 4 },
  value: { fontSize: 18, fontWeight: 'bold', color: '#fff', marginTop: 2 },
});