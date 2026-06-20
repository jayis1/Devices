// AlertsScreen — Alert history + acknowledge

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, TouchableOpacity } from 'react-native';
import { getAlerts, AlertEntry } from '../api';

const USER_ID = 1;

export default function AlertsScreen() {
  const [alerts, setAlerts] = useState<AlertEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const data = await getAlerts(USER_ID);
      setAlerts(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  const severityColor = (sev: string) => {
    if (sev === 'high') return '#F44336';
    if (sev === 'medium') return '#FF9800';
    return '#4CAF50';
  };

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <Text style={styles.header}>Alerts</Text>
      {alerts.length === 0 ? (
        <Text style={styles.empty}>No alerts — you're doing great! 🌿</Text>
      ) : (
        alerts.map((alert) => (
          <View key={alert.id} style={[styles.alertCard, { borderLeftColor: severityColor(alert.severity) }]}>
            <View style={styles.alertHeader}>
              <Text style={[styles.alertType, { color: severityColor(alert.severity) }]}>
                {alert.type.replace(/_/g, ' ').toUpperCase()}
              </Text>
              <Text style={styles.alertTime}>{new Date(alert.ts).toLocaleString()}</Text>
            </View>
            <Text style={styles.alertMessage}>{alert.message}</Text>
            <Text style={styles.alertSeverity}>Severity: {alert.severity}</Text>
            {!alert.acknowledged && (
              <TouchableOpacity style={styles.ackButton}>
                <Text style={styles.ackText}>Acknowledge</Text>
              </TouchableOpacity>
            )}
          </View>
        ))
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  empty: { color: '#999', textAlign: 'center', padding: 40, fontSize: 16 },
  alertCard: { backgroundColor: 'white', margin: 16, marginBottom: 8, padding: 16, borderRadius: 12, borderLeftWidth: 4 },
  alertHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  alertType: { fontSize: 14, fontWeight: 'bold' },
  alertTime: { fontSize: 12, color: '#999' },
  alertMessage: { fontSize: 14, color: '#333', marginBottom: 4 },
  alertSeverity: { fontSize: 12, color: '#666' },
  ackButton: { marginTop: 8, backgroundColor: '#6C63FF', padding: 8, borderRadius: 8, alignItems: 'center' },
  ackText: { color: 'white', fontSize: 12, fontWeight: 'bold' },
});