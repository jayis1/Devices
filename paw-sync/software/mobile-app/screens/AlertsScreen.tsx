// AlertsScreen — Alert history + acknowledge

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, RefreshControl, TouchableOpacity, Alert } from 'react-native';
import { getAlerts, AlertEntry, acknowledgeAlert } from '../api';

const PET_ID = 1;

const SEVERITY_COLORS: { [key: string]: string } = {
  high: '#F44336',
  medium: '#FF9800',
  low: '#4CAF50',
};

const ALERT_ICONS: { [key: string]: string } = {
  hrv_decline: '💔', hr_elevated: '❤️‍🔥', fever: '🌡️', lameness: '🦵',
  scratching: '🐕', appetite_loss: '🍽️', anxiety: '💜', high_risk: '⚠️',
};

export default function AlertsScreen() {
  const [alerts, setAlerts] = useState<AlertEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAlerts = useCallback(async () => {
    try {
      const data = await getAlerts(PET_ID);
      setAlerts(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => {
    fetchAlerts();
    const interval = setInterval(fetchAlerts, 30000);
    return () => clearInterval(interval);
  }, [fetchAlerts]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchAlerts();
    setRefreshing(false);
  };

  const handleAck = (alertId: number) => {
    Alert.alert('Acknowledge', 'Mark this alert as reviewed?', [
      { text: 'Cancel', style: 'cancel' },
      { text: 'OK', onPress: () => acknowledgeAlert(PET_ID, alertId) },
    ]);
  };

  const unackCount = alerts.filter(a => !a.acknowledged).length;

  return (
    <View style={styles.container}>
      <Text style={styles.header}>🔔 Alerts</Text>

      {unackCount > 0 && (
        <View style={styles.banner}>
          <Text style={styles.bannerText}>{unackCount} unacknowledged alert{unackCount > 1 ? 's' : ''}</Text>
        </View>
      )}

      <FlatList
        data={alerts}
        keyExtractor={(_, i) => i.toString()}
        refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
        renderItem={({ item }) => (
          <TouchableOpacity onPress={() => handleAck(item.id)} disabled={item.acknowledged}>
            <View style={[
              styles.alertRow,
              { borderLeftColor: SEVERITY_COLORS[item.severity] || '#999' },
              item.acknowledged && styles.acknowledged
            ]}>
              <Text style={styles.alertIcon}>
                {ALERT_ICONS[item.alert_type] || '⚠️'}
              </Text>
              <View style={styles.alertInfo}>
                <Text style={styles.alertMessage}>{item.message}</Text>
                <Text style={styles.alertTime}>{new Date(item.ts).toLocaleString()}</Text>
              </View>
              <View style={[styles.severityBadge, { backgroundColor: SEVERITY_COLORS[item.severity] || '#999' }]}>
                <Text style={styles.severityText}>{item.severity}</Text>
              </View>
            </View>
          </TouchableOpacity>
        )}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 22, fontWeight: 'bold', textAlign: 'center', marginVertical: 16 },
  banner: { backgroundColor: '#F44336', marginHorizontal: 16, padding: 12, borderRadius: 8, marginBottom: 8 },
  bannerText: { color: 'white', fontSize: 14, fontWeight: '600', textAlign: 'center' },
  alertRow: { flexDirection: 'row', alignItems: 'center', backgroundColor: 'white', marginHorizontal: 16, marginBottom: 4, padding: 12, borderRadius: 8, borderLeftWidth: 4 },
  acknowledged: { opacity: 0.5 },
  alertIcon: { fontSize: 24, marginRight: 12 },
  alertInfo: { flex: 1 },
  alertMessage: { fontSize: 14, fontWeight: '500', color: '#333' },
  alertTime: { fontSize: 11, color: '#999', marginTop: 2 },
  severityBadge: { paddingHorizontal: 8, paddingVertical: 4, borderRadius: 4 },
  severityText: { color: 'white', fontSize: 10, fontWeight: '600', textTransform: 'uppercase' },
});