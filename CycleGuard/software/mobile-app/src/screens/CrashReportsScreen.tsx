import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function CrashReportsScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Crash Reports</Text>
      <Text style={styles.subtitle}>Incident history for insurance & medical</Text>
      <View style={styles.emptyCard}>
        <Text style={styles.emptyText}>No crash incidents recorded.</Text>
        <Text style={styles.emptySubtext}>Stay safe out there. 🚴</Text>
      </View>
    </ScrollView>
  );
}
const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#0a0a0a' },
  title: { fontSize: 24, fontWeight: 'bold', color: '#2ecc71', marginBottom: 4 },
  subtitle: { fontSize: 14, color: '#7f8c8d', marginBottom: 20 },
  emptyCard: { backgroundColor: '#1a1a1a', padding: 30, borderRadius: 12, alignItems: 'center' },
  emptyText: { fontSize: 16, color: '#fff' },
  emptySubtext: { fontSize: 14, color: '#7f8c8d', marginTop: 8 },
});