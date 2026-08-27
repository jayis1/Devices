import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

export default function RoomsScreen() {
  return (
    <View style={styles.card}>
      <Text style={styles.heading}>Room Heatmap</Text>
      <Text style={styles.body}>Per-room dirt, humidity, wet-floor risk, and last-cleaned data for kitchen, bath, hall, and entry zones.</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: { backgroundColor: '#131C2E', borderRadius: 16, padding: 16 },
  heading: { color: 'white', fontSize: 20, fontWeight: '600', marginBottom: 8 },
  body: { color: '#B6C2D9', lineHeight: 20 },
});
