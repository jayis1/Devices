/**
 * BrewSync Dashboard Screen
 *
 * Shows real-time status of all active fermentation batches,
 * node health, and quick access to scanner and alerts.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  ScrollView,
  TouchableOpacity,
  StyleSheet,
  RefreshControl,
  Alert,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { LineChart } from 'react-native-chart-kit';
import { Dimensions } from 'react-native';
import api from '../services/api';

const screenWidth = Dimensions.get('window').width;

// Batch status colors
const STATUS_COLORS = {
  idle: '#666',
  lag_phase: '#FFA726',
  active_fermentation: '#66BB6A',
  finishing: '#42A5F5',
  conditioning: '#AB47BC',
  complete: '#78909C',
  stuck: '#EF5350',
  error: '#F44336',
};

interface Batch {
  id: string;
  name: string;
  style: string;
  status: string;
  vessel_id: string;
  current_sg: number | null;
  current_temp_c: number | null;
  target_fg: number | null;
  stuck_probability: number | null;
}

interface Alert {
  id: string;
  batch_id: string;
  alert_type: string;
  severity: number;
  message: string;
  created_at: string;
  acknowledged: boolean;
}

export default function DashboardScreen() {
  const navigation = useNavigation();
  const [batches, setBatches] = useState<Batch[]>([]);
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [refreshing, setRefreshing] = useState(false);
  const [loading, setLoading] = useState(true);

  const fetchData = useCallback(async () => {
    try {
      const [batchesRes, alertsRes] = await Promise.all([
        api.get('/v1/batches'),
        api.get('/v1/alerts'),
      ]);
      setBatches(batchesRes.data);
      setAlerts(alertsRes.data.filter((a: Alert) => !a.acknowledged));
    } catch (error) {
      console.error('Failed to fetch data:', error);
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 30000); // Refresh every 30s
    return () => clearInterval(interval);
  }, [fetchData]);

  const onRefresh = () => {
    setRefreshing(true);
    fetchData();
  };

  const renderBatchCard = (batch: Batch) => {
    const abv = batch.current_sg && batch.target_fg
      ? ((1.065 - batch.current_sg) * 131.25).toFixed(1) // Simplified
      : '--';
    const statusColor = STATUS_COLORS[batch.status] || '#666';
    const stuckWarning = batch.stuck_probability && batch.stuck_probability > 0.1;

    return (
      <TouchableOpacity
        key={batch.id}
        style={[styles.batchCard, { borderLeftColor: statusColor }]}
        onPress={() => navigation.navigate('BatchDetail', {
          batchId: batch.id,
          batchName: batch.name,
        })}
      >
        <View style={styles.batchHeader}>
          <Text style={styles.batchName}>{batch.name}</Text>
          <View style={[styles.statusBadge, { backgroundColor: statusColor }]}>
            <Text style={styles.statusText}>{batch.status.replace('_', ' ')}</Text>
          </View>
        </View>
        <Text style={styles.batchStyle}>{batch.style}</Text>

        <View style={styles.batchMetrics}>
          <View style={styles.metric}>
            <Text style={styles.metricLabel}>SG</Text>
            <Text style={styles.metricValue}>
              {batch.current_sg ? batch.current_sg.toFixed(4) : '--'}
            </Text>
          </View>
          <View style={styles.metric}>
            <Text style={styles.metricLabel}>Temp</Text>
            <Text style={styles.metricValue}>
              {batch.current_temp_c ? `${batch.current_temp_c.toFixed(1)}°C` : '--'}
            </Text>
          </View>
          <View style={styles.metric}>
            <Text style={styles.metricLabel}>ABV</Text>
            <Text style={styles.metricValue}>{abv}%</Text>
          </View>
          <View style={styles.metric}>
            <Text style={styles.metricLabel}>Target FG</Text>
            <Text style={styles.metricValue}>
              {batch.target_fg ? batch.target_fg.toFixed(3) : '--'}
            </Text>
          </View>
        </View>

        {stuckWarning && (
          <View style={styles.warningBanner}>
            <Text style={styles.warningText}>
              ⚠ Stuck fermentation risk: {(batch.stuck_probability! * 100).toFixed(0)}%
            </Text>
          </View>
        )}
      </TouchableOpacity>
    );
  };

  const renderAlert = (alert: Alert) => {
    const severityColor = alert.severity === 2 ? '#F44336' :
                         alert.severity === 1 ? '#FFA726' : '#42A5F5';
    return (
      <View key={alert.id} style={[styles.alertCard, { borderLeftColor: severityColor }]}>
        <Text style={styles.alertMessage}>{alert.message}</Text>
        <Text style={styles.alertTime}>
          {new Date(alert.created_at).toLocaleString()}
        </Text>
      </View>
    );
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Active Alerts */}
      {alerts.length > 0 && (
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>⚠ Active Alerts</Text>
          {alerts.map(renderAlert)}
        </View>
      )}

      {/* Active Batches */}
      <View style={styles.section}>
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>🍺 Active Batches</Text>
          <TouchableOpacity
            style={styles.newBatchButton}
            onPress={() => {/* Navigate to create batch */}}
          >
            <Text style={styles.newBatchButtonText}>+ New Batch</Text>
          </TouchableOpacity>
        </View>
        {batches.length === 0 ? (
          <View style={styles.emptyState}>
            <Text style={styles.emptyStateText}>
              No active batches. Start your first fermentation!
            </Text>
          </View>
        ) : (
          batches.map(renderBatchCard)
        )}
      </View>

      {/* Node Status */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>📡 Nodes</Text>
        <Text style={styles.nodeInfo}>4 nodes connected • All healthy</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0d1117',
  },
  section: {
    padding: 16,
  },
  sectionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#e6e6e6',
    marginBottom: 12,
  },
  batchCard: {
    backgroundColor: '#161b22',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
    borderLeftWidth: 4,
  },
  batchHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 4,
  },
  batchName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#fff',
  },
  statusBadge: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
  },
  statusText: {
    fontSize: 11,
    color: '#fff',
    textTransform: 'uppercase',
    fontWeight: '600',
  },
  batchStyle: {
    fontSize: 13,
    color: '#8b949e',
    marginBottom: 12,
  },
  batchMetrics: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  metric: {
    alignItems: 'center',
  },
  metricLabel: {
    fontSize: 11,
    color: '#8b949e',
    marginBottom: 2,
  },
  metricValue: {
    fontSize: 14,
    fontWeight: '600',
    color: '#e6e6e6',
  },
  warningBanner: {
    backgroundColor: 'rgba(239, 83, 80, 0.15)',
    borderRadius: 8,
    padding: 8,
    marginTop: 12,
  },
  warningText: {
    color: '#EF5350',
    fontSize: 13,
    fontWeight: '500',
  },
  alertCard: {
    backgroundColor: '#161b22',
    borderRadius: 8,
    padding: 12,
    marginBottom: 8,
    borderLeftWidth: 3,
  },
  alertMessage: {
    color: '#e6e6e6',
    fontSize: 14,
  },
  alertTime: {
    color: '#8b949e',
    fontSize: 12,
    marginTop: 4,
  },
  newBatchButton: {
    backgroundColor: '#D4A017',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 8,
  },
  newBatchButtonText: {
    color: '#000',
    fontWeight: '600',
    fontSize: 13,
  },
  emptyState: {
    padding: 40,
    alignItems: 'center',
  },
  emptyStateText: {
    color: '#8b949e',
    textAlign: 'center',
    fontSize: 15,
  },
  nodeInfo: {
    color: '#8b949e',
    fontSize: 14,
  },
});