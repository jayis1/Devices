// GreenPulse AlertsScreen — real-time alert feed

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, FlatList, StyleSheet, TouchableOpacity } from 'react-native';
import { getAlerts, AlertEntry, AlertWebSocket } from '../api';

export default function AlertsScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [alerts, setAlerts] = useState<AlertEntry[]>([]);

  const load = useCallback(async () => {
    const data = await getAlerts(userId);
    setAlerts(data);
  }, [userId]);

  useEffect(() => {
    load();
    const ws = new AlertWebSocket(userId, (alert) => {
      setAlerts(prev => [alert, ...prev].slice(0, 100));
    });
    ws.connect();
    return () => ws.disconnect();
  }, [load]);

  const severityColor = (s: string) => {
    switch (s) {
      case 'high': return '#F44336';
      case 'medium': return '#FF9800';
      default: return '#2196F3';
    }
  };

  const alertIcon = (type: string) => {
    switch (type) {
      case 'low_moisture': return '💧';
      case 'disease': return '🔬';
      case 'low_light': return '☀️';
      case 'low_battery': return '🔋';
      case 'pest': return '🐛';
      default: return '🌿';
    }
  };

  const renderItem = ({ item }: { item: AlertEntry }) => (
    <View style={styles.alertCard}>
      <Text style={styles.alertIcon}>{alertIcon(item.type)}</Text>
      <View style={{ flex: 1 }}>
        <Text style={styles.alertMessage}>{item.message}</Text>
        <Text style={styles.alertTime}>{item.ts.slice(0, 16)}</Text>
      </View>
      <View style={[styles.severityBar,
        { backgroundColor: severityColor(item.severity) }]} />
    </View>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>🔔 Alerts</Text>
      <FlatList
        data={alerts}
        keyExtractor={(a) => a.id.toString()}
        renderItem={renderItem}
        contentContainerStyle={{ paddingBottom: 20 }}
        refreshing={false}
        onRefresh={load}
      />
      {alerts.length === 0 && (
        <Text style={styles.empty}>No alerts — your plants are happy! 🌱</Text>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#2E7D32' },
  alertCard: { flexDirection: 'row', alignItems: 'center', backgroundColor: '#fff',
    marginHorizontal: 16, marginBottom: 8, borderRadius: 12, padding: 14,
    borderLeftWidth: 0 },
  alertIcon: { fontSize: 28, marginRight: 12 },
  alertMessage: { fontSize: 15, color: '#333', fontWeight: '500' },
  alertTime: { fontSize: 12, color: '#999', marginTop: 2 },
  severityBar: { width: 4, height: '100%', position: 'absolute',
    left: 0, top: 0, bottom: 0, borderRadius: 2 },
  empty: { textAlign: 'center', color: '#999', marginTop: 40, fontSize: 16 },
});