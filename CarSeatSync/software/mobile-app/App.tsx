import React from 'react';
import { SafeAreaView, ScrollView, Text, View, StyleSheet } from 'react-native';

const card = (title: string, body: string) => (
  <View style={styles.card} key={title}>
    <Text style={styles.cardTitle}>{title}</Text>
    <Text style={styles.body}>{body}</Text>
  </View>
);

export default function App() {
  const cards = [
    ['Live Trip', 'Child in seat: yes. Ignition: off. Cabin temp: 40.2 C. Caregiver nearby: no.'],
    ['Alert Ladder', '1) Push alert 2) Vehicle siren 3) LTE call tree 4) Emergency escalation.'],
    ['Destination Checklist', 'Child, bag, medicine, handoff beacon confirmation.'],
    ['History', 'Trips scored by harness quality, unload completion, and heat exposure.'],
  ];

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.title}>CarSeatSync</Text>
        <Text style={styles.subtitle}>Infant and child vehicle safety dashboard</Text>
        {cards.map(([title, body]) => card(title, body))}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#08111f' },
  content: { padding: 20, gap: 16 },
  title: { color: '#f8fafc', fontSize: 28, fontWeight: '700' },
  subtitle: { color: '#93c5fd', fontSize: 16, marginBottom: 8 },
  card: { backgroundColor: '#132238', borderRadius: 14, padding: 16 },
  cardTitle: { color: '#f8fafc', fontSize: 18, fontWeight: '600', marginBottom: 8 },
  body: { color: '#cbd5e1', lineHeight: 20 },
});
