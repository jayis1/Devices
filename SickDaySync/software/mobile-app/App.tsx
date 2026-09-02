import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import { riskCardText } from './src/api';

const App = () => {
  const cards = riskCardText();
  return (
    <SafeAreaView>
      <ScrollView contentInsetAdjustmentBehavior="automatic" style={{ padding: 16 }}>
        <Text style={{ fontSize: 28, fontWeight: '700', marginBottom: 12 }}>SickDaySync</Text>
        {cards.map((card, index) => (
          <View key={index} style={{ padding: 12, marginBottom: 12, borderWidth: 1, borderRadius: 10 }}>
            <Text style={{ fontSize: 20, fontWeight: '600' }}>{card.title}</Text>
            <Text>{card.body}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
};

export default App;
