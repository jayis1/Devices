// SeizureSync — Alerts screen (real-time alert history)
import React, { useState, useEffect } from 'react';
import { View, Text, FlatList, StyleSheet } from 'react-native';

export default function AlertsScreen() {
  const [alerts, setAlerts] = useState<any[]>([]);

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Alerts</Text>
      <FlatList data={alerts} renderItem={({item}) => (
        <View style={[styles.alertCard, { borderLeftColor: item.color }]}>
          <Text style={styles.type}>{item.type}</Text>
          <Text style={styles.time}>{item.time}</Text>
        </View>
      )} keyExtractor={(item, i) => i.toString()}
        ListEmptyComponent={<Text style={styles.empty}>No alerts</Text>} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', padding: 16, color: '#0A1144' },
  alertCard: { margin: 8, padding: 16, backgroundColor: '#fff', borderRadius: 8,
               borderLeftWidth: 4 },
  type: { fontSize: 18, fontWeight: 'bold' },
  time: { fontSize: 12, color: '#999' },
  empty: { textAlign: 'center', marginTop: 40, color: '#999' },
});