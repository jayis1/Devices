import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  FlatList,
} from 'react-native';

const API_BASE = 'http://soundnest-hub.local:8000/api/v1';

interface AlertEvent {
  id: string;
  timestamp: string;
  sound_class: number;
  sound_class_name: string;
  confidence: number;
  spl_dba: number;
  room_name: string;
  priority: string;
  action_taken: string;
}

export default function AlertsScreen() {
  const [alerts, setAlerts] = useState<AlertEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState<'all' | 'critical' | 'high' | 'medium'>('all');

  useEffect(() => {
    fetchAlerts();
    const interval = setInterval(fetchAlerts, 10000);
    return () => clearInterval(interval);
  }, [filter]);

  const fetchAlerts = async () => {
    try {
      let url = `${API_BASE}/events?limit=100`;
      if (filter !== 'all') {
        url += `&min_confidence=${
          filter === 'critical' ? 90 : filter === 'high' ? 70 : 50
        }`;
      }
      const response = await fetch(url);
      if (response.ok) {
        const data = await response.json();
        setAlerts(data);
      }
    } catch (error) {
      console.error('Failed to fetch alerts:', error);
    } finally {
      setLoading(false);
    }
  };

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'critical': return '#FF4444';
      case 'high': return '#FF8C00';
      case 'medium': return '#FFD700';
      case 'low': return '#4ECDC4';
      default: return '#8A9BA8';
    }
  };

  const formatTime = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  };

  const renderAlert = useCallback(({ item }: { item: AlertEvent }) => (
    <View style={styles.alertCard}>
      <View style={styles.alertHeader}>
        <View style={[styles.priorityDot, { backgroundColor: getPriorityColor(item.priority) }]} />
        <Text style={styles.alertTime}>{formatTime(item.timestamp)}</Text>
        <Text style={styles.alertRoom}>{item.room_name}</Text>
      </View>
      <Text style={styles.alertTitle}>{item.sound_class_name}</Text>
      <View style={styles.alertDetails}>
        <Text style={styles.alertDetail}>
          {item.spl_dba.toFixed(1)} dB(A)
        </Text>
        <Text style={styles.alertDetail}>
          {item.confidence.toFixed(0)}% confidence
        </Text>
      </View>
      {item.action_taken && (
        <Text style={styles.alertAction}>
          Action: {item.action_taken}
        </Text>
      )}
    </View>
  ), []);

  return (
    <View style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.title}>Alerts</Text>
        <Text style={styles.subtitle}>
          {alerts.length} events
        </Text>
      </View>

      {/* Filter Tabs */}
      <View style={styles.filterBar}>
        {(['all', 'critical', 'high', 'medium'] as const).map(f => (
          <TouchableOpacity
            key={f}
            style={[styles.filterTab, filter === f && styles.filterTabActive]}
            onPress={() => setFilter(f)}
          >
            <Text style={[styles.filterText, filter === f && styles.filterTextActive]}>
              {f.charAt(0).toUpperCase() + f.slice(1)}
            </Text>
          </TouchableOpacity>
        ))}
      </View>

      {/* Alert List */}
      <FlatList
        data={alerts}
        keyExtractor={item => item.id}
        renderItem={renderAlert}
        contentContainerStyle={styles.alertList}
        refreshing={loading}
        onRefresh={fetchAlerts}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0D1B2A',
  },
  header: {
    padding: 20,
    paddingTop: 60,
    backgroundColor: '#1B2838',
  },
  title: {
    fontSize: 28,
    fontWeight: '700',
    color: '#FFFFFF',
  },
  subtitle: {
    fontSize: 14,
    color: '#8A9BA8',
    marginTop: 4,
  },
  filterBar: {
    flexDirection: 'row',
    padding: 12,
    backgroundColor: '#1B2838',
  },
  filterTab: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 20,
    marginRight: 8,
    backgroundColor: '#2A3A4A',
  },
  filterTabActive: {
    backgroundColor: '#4ECDC4',
  },
  filterText: {
    fontSize: 14,
    color: '#8A9BA8',
    fontWeight: '500',
  },
  filterTextActive: {
    color: '#0D1B2A',
  },
  alertList: {
    padding: 12,
  },
  alertCard: {
    backgroundColor: '#1B2838',
    borderRadius: 12,
    padding: 16,
    marginBottom: 8,
  },
  alertHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 8,
  },
  priorityDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
    marginRight: 8,
  },
  alertTime: {
    fontSize: 12,
    color: '#8A9BA8',
    marginRight: 12,
  },
  alertRoom: {
    fontSize: 12,
    color: '#4ECDC4',
  },
  alertTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#FFFFFF',
    marginBottom: 4,
  },
  alertDetails: {
    flexDirection: 'row',
    gap: 16,
  },
  alertDetail: {
    fontSize: 14,
    color: '#8A9BA8',
  },
  alertAction: {
    fontSize: 12,
    color: '#4ECDC4',
    marginTop: 4,
    fontStyle: 'italic',
  },
});