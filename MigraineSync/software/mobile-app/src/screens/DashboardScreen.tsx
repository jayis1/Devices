/**
 * MigraineSync — Dashboard Screen
 * =================================
 * Shows current migraine risk level, 48-hour forecast curve,
 * top trigger today, and quick action buttons.
 *
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, RefreshControl, ScrollView, TouchableOpacity } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { getRisk, RiskForecast, logEvent } from '../services/api';

export default function DashboardScreen() {
  const [risk, setRisk] = useState<RiskForecast | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const fetchRisk = useCallback(async () => {
    try {
      const data = await getRisk();
      setRisk(data);
    } catch (e) {
      console.error('Failed to fetch risk:', e);
    }
  }, []);

  useEffect(() => {
    fetchRisk();
    const interval = setInterval(fetchRisk, 60000); // refresh every 60s
    return () => clearInterval(interval);
  }, [fetchRisk]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchRisk();
    setRefreshing(false);
  }, [fetchRisk]);

  const riskColor = risk?.risk_level === 'high' ? '#E74C3C'
    : risk?.risk_level === 'moderate' ? '#F39C12'
    : '#27AE60';

  const handleLogMigraine = async () => {
    await logEvent('migraine_onset', 7, 'User reported migraine from dashboard');
    alert('Migraine logged. The system will use this to improve predictions.');
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.title}>MigraineSync</Text>
        <Text style={styles.subtitle}>AI-Powered Migraine Prevention</Text>
      </View>

      {/* Risk Gauge */}
      <View style={[styles.riskCard, { borderColor: riskColor }]}>
        <Text style={styles.riskLabel}>48-Hour Risk</Text>
        <Text style={[styles.riskScore, { color: riskColor }]}>
          {risk ? `${risk.risk_score.toFixed(1)}%` : '—'}
        </Text>
        <Text style={[styles.riskLevel, { color: riskColor }]}>
          {risk?.risk_level?.toUpperCase() || 'LOADING'}
        </Text>
        <Text style={styles.confidence}>
          Confidence: {risk ? `${(risk.confidence * 100).toFixed(0)}%` : '—'}
        </Text>
        <Text style={styles.trend}>
          Trend: {risk?.trend || '—'}
        </Text>
      </View>

      {/* Contributing Factors */}
      {risk && risk.contributing_factors.length > 0 && (
        <View style={styles.factorsCard}>
          <Text style={styles.sectionTitle}>Contributing Factors</Text>
          {risk.contributing_factors.map((f, i) => (
            <View key={i} style={styles.factorRow}>
              <View style={styles.factorInfo}>
                <Text style={styles.factorName}>
                  {f.factor.replace(/_/g, ' ').toUpperCase()}
                </Text>
                <Text style={styles.factorValue}>{f.value}</Text>
              </View>
              <Text style={styles.factorPct}>{f.contribution_pct.toFixed(1)}%</Text>
            </View>
          ))}
        </View>
      )}

      {/* Recommended Action */}
      {risk?.recommended_action && (
        <View style={styles.actionCard}>
          <Ionicons name="bulb" size={24} color="#6C5CE7" />
          <Text style={styles.actionText}>{risk.recommended_action}</Text>
        </View>
      )}

      {/* Quick Actions */}
      <View style={styles.quickActions}>
        <TouchableOpacity style={styles.actionButton} onPress={handleLogMigraine}>
          <Ionicons name="warning" size={32} color="#E74C3C" />
          <Text style={styles.actionButtonText}>Log Migraine</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={styles.actionButton}
          onPress={() => logEvent('medication', 1, 'Medication taken')}
        >
          <Ionicons name="medkit" size={32} color="#3498DB" />
          <Text style={styles.actionButtonText}>Log Meds</Text>
        </TouchableOpacity>
      </View>

      {/* Last Updated */}
      <Text style={styles.lastUpdated}>
        Last updated: {risk?.last_updated || '—'}
      </Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1A1A2E' },
  header: { padding: 20, paddingTop: 50 },
  title: { fontSize: 28, fontWeight: 'bold', color: '#6C5CE7' },
  subtitle: { fontSize: 14, color: '#636E72', marginTop: 4 },
  riskCard: {
    margin: 20, padding: 24, borderRadius: 16,
    backgroundColor: '#16213E', borderWidth: 2, alignItems: 'center',
  },
  riskLabel: { fontSize: 16, color: '#636E72', marginBottom: 8 },
  riskScore: { fontSize: 56, fontWeight: 'bold' },
  riskLevel: { fontSize: 20, fontWeight: '600', marginTop: 4 },
  confidence: { fontSize: 12, color: '#636E72', marginTop: 8 },
  trend: { fontSize: 12, color: '#636E72' },
  factorsCard: { margin: 20, padding: 16, borderRadius: 12, backgroundColor: '#16213E' },
  sectionTitle: { fontSize: 16, fontWeight: '600', color: '#FFFFFF', marginBottom: 12 },
  factorRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8 },
  factorInfo: { flex: 1 },
  factorName: { fontSize: 12, color: '#A0A0B0', fontWeight: '500' },
  factorValue: { fontSize: 11, color: '#636E72', marginTop: 2 },
  factorPct: { fontSize: 18, fontWeight: 'bold', color: '#6C5CE7' },
  actionCard: {
    margin: 20, padding: 16, borderRadius: 12,
    backgroundColor: '#16213E', flexDirection: 'row', alignItems: 'center',
  },
  actionText: { flex: 1, fontSize: 14, color: '#FFFFFF', marginLeft: 12, flexWrap: 'wrap' },
  quickActions: { flexDirection: 'row', justifyContent: 'space-around', margin: 20 },
  actionButton: { alignItems: 'center', padding: 16 },
  actionButtonText: { fontSize: 12, color: '#A0A0B0', marginTop: 8 },
  lastUpdated: { textAlign: 'center', fontSize: 10, color: '#636E72', marginBottom: 20 },
});