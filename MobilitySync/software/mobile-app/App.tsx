import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';

import { mockSummary } from './src/mock';

export default function App() {
  return (
    <SafeAreaView style={{ flex: 1, backgroundColor: '#08111f' }}>
      <ScrollView contentContainerStyle={{ padding: 20 }}>
        <Text style={{ color: 'white', fontSize: 28, fontWeight: '700' }}>MobilitySync</Text>
        <Text style={{ color: '#9ab0cf', marginTop: 8 }}>Home mobility assistance dashboard</Text>
        {mockSummary.cards.map((card) => (
          <View key={card.label} style={{ backgroundColor: '#12233d', padding: 16, borderRadius: 16, marginTop: 16 }}>
            <Text style={{ color: '#8aa4d1', fontSize: 13 }}>{card.label}</Text>
            <Text style={{ color: 'white', fontSize: 24, fontWeight: '700', marginTop: 6 }}>{card.value}</Text>
            <Text style={{ color: '#b3c3e0', marginTop: 4 }}>{card.detail}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}
