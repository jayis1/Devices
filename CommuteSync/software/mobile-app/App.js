import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';

const card = { backgroundColor: '#101820', padding: 16, borderRadius: 14, marginBottom: 12 };
const title = { color: '#f4f7fb', fontSize: 20, fontWeight: '700', marginBottom: 8 };
const body = { color: '#d7dde8', fontSize: 15, marginBottom: 4 };

export default function App() {
  const actions = [
    'Tap badge before leaving',
    'Lower-PM route saves 11 exposure points',
    'Bike lock armed and bag present at desk',
  ];

  return (
    <SafeAreaView style={{ flex: 1, backgroundColor: '#0b0f14' }}>
      <ScrollView contentContainerStyle={{ padding: 16 }}>
        <View style={card}>
          <Text style={title}>CommuteSync</Text>
          <Text style={body}>Readiness risk: medium</Text>
          <Text style={body}>Lateness risk: medium</Text>
          <Text style={body}>Exposure risk: low</Text>
          <Text style={body}>Theft risk: low</Text>
        </View>
        <View style={card}>
          <Text style={title}>Recommended actions</Text>
          {actions.map((action) => (
            <Text key={action} style={body}>• {action}</Text>
          ))}
        </View>
        <View style={card}>
          <Text style={title}>This week</Text>
          <Text style={body}>18 minutes saved by early delay warnings</Text>
          <Text style={body}>3 forgotten-item events prevented</Text>
          <Text style={body}>22% lower PM2.5 burden on alternate routes</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}
