import React from 'react';
import { SafeAreaView, ScrollView, Text, View, StyleSheet } from 'react-native';

const Card = ({ title, value, accent }: { title: string; value: string; accent: string }) => (
  <View style={[styles.card, { borderLeftColor: accent }]}> 
    <Text style={styles.cardTitle}>{title}</Text>
    <Text style={styles.cardValue}>{value}</Text>
  </View>
);

export default function App() {
  return (
    <SafeAreaView style={styles.root}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.title}>DrainSync</Text>
        <Text style={styles.subtitle}>Home drain health, odor prevention, and sewer backup defense.</Text>
        <Card title="Backup Risk" value="Green • 18%" accent="#16a34a" />
        <Card title="Highest Clog Risk" value="Kitchen West • 74%" accent="#f59e0b" />
        <Card title="Trap Prime Needed" value="Basement Floor Drain • 300 mL" accent="#0ea5e9" />
        <Card title="Backwater Valve" value="Open • Healthy" accent="#6366f1" />
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#08111d' },
  content: { padding: 20, gap: 14 },
  title: { fontSize: 34, color: '#f8fafc', fontWeight: '700' },
  subtitle: { color: '#cbd5e1', marginBottom: 8 },
  card: {
    backgroundColor: '#111c2d',
    borderRadius: 16,
    padding: 16,
    borderLeftWidth: 6,
  },
  cardTitle: { color: '#94a3b8', fontSize: 13, textTransform: 'uppercase', marginBottom: 6 },
  cardValue: { color: '#f8fafc', fontSize: 22, fontWeight: '600' },
});
