import React from 'react';
import { Text, View } from 'react-native';

export default function DashboardScreen() {
  const cards = [
    'Meal safety score',
    'Surface strip status',
    'Lunchbox integrity',
    'EpiPen readiness',
  ];

  return (
    <View>
      {cards.map((card) => (
        <View key={card} style={{ padding: 12, borderWidth: 1, borderColor: '#ddd', borderRadius: 12, marginBottom: 10 }}>
          <Text style={{ fontWeight: '600' }}>{card}</Text>
          <Text>Live household summary placeholder</Text>
        </View>
      ))}
    </View>
  );
}
