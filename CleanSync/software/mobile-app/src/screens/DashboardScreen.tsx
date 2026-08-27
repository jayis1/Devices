import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

export default function DashboardScreen() {
  return (
    <View style={styles.card}>
      <Text style={styles.heading}>Dashboard</Text>
      <Text style={styles.body}>Whole-home cleanliness score, active alerts, and the next best cleaning mission are shown here.</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: { backgroundColor: '#131C2E', borderRadius: 16, padding: 16 },
  heading: { color: 'white', fontSize: 20, fontWeight: '600', marginBottom: 8 },
  body: { color: '#B6C2D9', lineHeight: 20 },
});
