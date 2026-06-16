/**
 * PowerPulse — Alerts Screen (React Native)
 */

import React from 'react';
import { View, Text, ScrollView, StyleSheet, TouchableOpacity } from 'react-native';
import { useAlerts } from '../hooks/useAlerts';

const SEVERITY_COLORS: Record<number, string> = {
  1: '#ffaa00',  // low - yellow
  2: '#ff6600',  // medium - orange
  3: '#ff4444',  // high - red
  4: '#ff0000',  // critical - bright red
};

const SEVERITY_LABELS: Record<number, string> = {
  1: 'LOW', 2: 'MEDIUM', 3: 'HIGH', 4: 'CRITICAL',
};

export default function AlertsScreen() {
  const { alerts, unreadCount, acknowledge } = useAlerts();
  
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Alerts</Text>
      {unreadCount > 0 && (
        <Text style={styles.unread}>{unreadCount} unread</Text>
      )}
      
      {alerts.length === 0 ? (
        <View style={styles.empty}>
          <Text style={styles.emptyText}>No alerts</Text>
          <Text style={styles.emptySubtext}>Your home is safe!</Text>
        </View>
      ) : (
        alerts.map(alert => (
          <View key={alert.id} style={[
            styles.alertCard,
            { borderLeftColor: SEVERITY_COLORS[alert.severity] || '#888' }
          ]}>
            <View style={styles.alertHeader}>
              <Text style={[styles.alertType, { color: SEVERITY_COLORS[alert.severity] || '#888' }]}>
                {alert.alert_type.replace('_', ' ').toUpperCase()}
              </Text>
              <Text style={styles.alertSeverity}>
                {SEVERITY_LABELS[alert.severity] || 'UNKNOWN'}
              </Text>
            </View>
            <Text style={styles.alertMessage}>{alert.message}</Text>
            <Text style={styles.alertTime}>
              {new Date(alert.timestamp).toLocaleString()}
            </Text>
            {!alert.acknowledged && (
              <TouchableOpacity 
                style={styles.ackButton}
                onPress={() => acknowledge(alert.id)}
              >
                <Text style={styles.ackButtonText}>Acknowledge</Text>
              </TouchableOpacity>
            )}
          </View>
        ))
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a0a23', padding: 16 },
  title: { color: '#fff', fontSize: 24, fontWeight: '700' },
  unread: { color: '#ff4444', fontSize: 14, marginTop: 4, marginBottom: 16 },
  empty: { alignItems: 'center', marginTop: 80 },
  emptyText: { color: '#888', fontSize: 18 },
  emptySubtext: { color: '#555', fontSize: 14, marginTop: 8 },
  alertCard: { 
    backgroundColor: '#16213e', borderRadius: 8, padding: 16, 
    marginBottom: 12, borderLeftWidth: 4 
  },
  alertHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 4 },
  alertType: { fontSize: 14, fontWeight: '700' },
  alertSeverity: { color: '#888', fontSize: 12, fontWeight: '600' },
  alertMessage: { color: '#ccc', fontSize: 14, lineHeight: 20 },
  alertTime: { color: '#666', fontSize: 12, marginTop: 4 },
  ackButton: { 
    backgroundColor: '#00d4aa', borderRadius: 6, paddingVertical: 8, 
    paddingHorizontal: 16, marginTop: 8, alignSelf: 'flex-start' 
  },
  ackButtonText: { color: '#000', fontSize: 12, fontWeight: '700' },
});