// SkinSync AlertsScreen — real-time alerts

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl } from 'react-native';
import { getAlerts, AlertEntry } from '../api';

const SEVERITY_COLORS: Record<string, string> = {
  high: '#F44336',
  medium: '#FF9800',
  low: '#2196F3',
};

const TYPE_ICONS: Record<string, string> = {
  uv_danger: '🔴', uv_warning: '🟠', uv_caution: '🟡',
  lesion_change: '⚠', low_battery: '🔋', low_product: '🧴',
};

export default function AlertsScreen({ route }: any) {
  const userId = route?.params?.userId ?? 1;
  const [alerts, setAlerts] = useState<AlertEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const load = useCallback(async () => {
    try {
      const data = await getAlerts(userId);
      setAlerts(data);
    } catch (e) {
      console.error('Failed to load alerts:', e);
    }
    setRefreshing(false);
  }, [userId]);

  useEffect(() => { load(); }, [load]);

  return (
    <ScrollView style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={load} />}>
      <Text style={styles.title}>🔔 Alerts</Text>

      {alerts.map((alert) => (
        <View key={alert.id} style={[styles.alertCard,
          { borderLeftColor: SEVERITY_COLORS[alert.severity] || '#999' }]}>
          <View style={styles.alertHeader}>
            <Text style={styles.alertIcon}>
              {TYPE_ICONS[alert.type] || '📋'}
            </Text>
            <Text style={[styles.alertSeverity,
              { color: SEVERITY_COLORS[alert.severity] }]}>
              {alert.severity.toUpperCase()}
            </Text>
          </View>
          <Text style={styles.alertMessage}>{alert.message}</Text>
          <Text style={styles.alertTime}>
            {new Date(alert.ts).toLocaleString()}
          </Text>
        </View>
      ))}

      {alerts.length === 0 && (
        <Text style={styles.empty}>No alerts. Your skin is happy! 🎉</Text>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#FAFAFA' },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, color: '#E91E63' },
  alertCard: { backgroundColor: '#fff', padding: 16, marginHorizontal: 16, marginBottom: 8,
    borderRadius: 10, borderLeftWidth: 4 },
  alertHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  alertIcon: { fontSize: 24 },
  alertSeverity: { fontSize: 12, fontWeight: 'bold' },
  alertMessage: { fontSize: 15, color: '#333' },
  alertTime: { fontSize: 12, color: '#999', marginTop: 8 },
  empty: { textAlign: 'center', color: '#999', marginTop: 40, fontSize: 16 },
});