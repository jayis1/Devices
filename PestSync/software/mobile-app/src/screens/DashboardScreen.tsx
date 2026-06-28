/**
 * DashboardScreen — Live overview, alerts, infestation risk
 */
import React, { useEffect, useState } from 'react';
import { View, Text, ScrollView, StyleSheet, Alert } from 'react-native';
import { useDispatch, useSelector } from 'react-redux';
import RiskGauge from '../components/RiskGauge';
import ActivityChart from '../components/ActivityChart';
import PestIcon from '../components/PestIcon';
import { fetchDashboard } from '../api/client';

export default function DashboardScreen() {
  const [data, setData] = useState<any>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchDashboard()
      .then(setData)
      .catch(() => {})
      .finally(() => setLoading(false));
  }, []);

  if (loading || !data) {
    return (
      <View style={styles.center}>
        <Text style={styles.loading}>Loading PestSync...</Text>
      </View>
    );
  }

  return (
    <ScrollView style={styles.container}>
      {/* Infestation Risk Gauge */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Infestation Risk (30-day)</Text>
        <RiskGauge risk={data.infestationRisk} />
        <Text style={styles.riskLabel}>{data.riskLevel}</Text>
        <Text style={styles.recommendation}>{data.recommendation}</Text>
      </View>

      {/* Latest Detection */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Latest Detection</Text>
        <View style={styles.detectionRow}>
          <PestIcon pestClass={data.latestDetection?.pestClass} size={48} />
          <View style={styles.detectionInfo}>
            <Text style={styles.pestName}>{data.latestDetection?.pestName}</Text>
            <Text style={styles.pestMeta}>
              {data.latestDetection?.confidence}% confidence · {data.latestDetection?.location}
            </Text>
            <Text style={styles.pestTime}>{data.latestDetection?.timestamp}</Text>
          </View>
        </View>
      </View>

      {/* 24h Activity Chart */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Activity (24 hours)</Text>
        <ActivityChart data={data.activityHeatmap || []} />
        <Text style={styles.patternLabel}>
          Pattern: {data.activityPattern} · Peak: {data.peakHour}:00
        </Text>
      </View>

      {/* Alerts */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Alerts ({data.alerts?.length || 0})</Text>
        {data.alerts?.map((alert: any, i: number) => (
          <View key={i} style={[styles.alert, alert.severity === 2 && styles.alertCritical]}>
            <Text style={styles.alertTitle}>
              {alert.severity === 2 ? '🚨' : alert.severity === 1 ? '⚠️' : 'ℹ️'} {alert.title}
            </Text>
            <Text style={styles.alertMsg}>{alert.message}</Text>
          </View>
        ))}
      </View>

      {/* Quick Stats */}
      <View style={styles.statsRow}>
        <View style={styles.statBox}>
          <Text style={styles.statValue}>{data.totalDetections || 0}</Text>
          <Text style={styles.statLabel}>Detections</Text>
        </View>
        <View style={styles.statBox}>
          <Text style={styles.statValue}>{data.activeTraps || 0}</Text>
          <Text style={styles.statLabel}>Traps</Text>
        </View>
        <View style={styles.statBox}>
          <Text style={styles.statValue}>{data.activeDeterrents || 0}</Text>
          <Text style={styles.statLabel}>Deterrents</Text>
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  center: { flex: 1, justifyContent: 'center', alignItems: 'center', backgroundColor: '#1a1a2e' },
  loading: { color: '#fff', fontSize: 18 },
  card: { backgroundColor: '#16213e', margin: 10, padding: 15, borderRadius: 12 },
  cardTitle: { color: '#e0e0e0', fontSize: 16, fontWeight: 'bold', marginBottom: 10 },
  riskLabel: { color: '#e74c3c', fontSize: 20, fontWeight: 'bold', textAlign: 'center', marginTop: 5 },
  recommendation: { color: '#bdc3c7', fontSize: 12, marginTop: 8, lineHeight: 18 },
  detectionRow: { flexDirection: 'row', alignItems: 'center' },
  detectionInfo: { marginLeft: 15, flex: 1 },
  pestName: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  pestMeta: { color: '#95a5a6', fontSize: 13, marginTop: 2 },
  pestTime: { color: '#7f8c8d', fontSize: 12, marginTop: 2 },
  patternLabel: { color: '#95a5a6', fontSize: 12, marginTop: 8, textAlign: 'center' },
  alert: { backgroundColor: '#0f3460', padding: 10, borderRadius: 8, marginBottom: 8 },
  alertCritical: { backgroundColor: '#4a1525' },
  alertTitle: { color: '#fff', fontSize: 14, fontWeight: 'bold' },
  alertMsg: { color: '#bdc3c7', fontSize: 12, marginTop: 4 },
  statsRow: { flexDirection: 'row', justifyContent: 'space-around', margin: 10 },
  statBox: { alignItems: 'center', backgroundColor: '#16213e', padding: 15, borderRadius: 12, minWidth: 90 },
  statValue: { color: '#e74c3c', fontSize: 28, fontWeight: 'bold' },
  statLabel: { color: '#95a5a6', fontSize: 12, marginTop: 4 },
});