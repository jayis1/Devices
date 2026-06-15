/**
 * ErgoFlow — Posture History Screen
 * Shows posture score time series and trends
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import React, { useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import useErgoFlowStore from '../state/ErgoFlowContext';

export default function PostureHistoryScreen() {
  const { postureHistory, fetchPostureHistory, fetchRsiRisk, rsiRiskScore, rsiRiskLevel } = useErgoFlowStore();

  useEffect(() => {
    fetchPostureHistory(60);
    fetchRsiRisk();
  }, []);

  // Simple ASCII chart (in production, use react-native-chart-kit)
  const renderChart = () => {
    if (postureHistory.length === 0) {
      return <Text style={styles.emptyText}>No posture data yet</Text>;
    }

    const scores = postureHistory.map(r => r.score);
    const maxScore = Math.max(...scores);
    const minScore = Math.min(...scores);
    const avg = scores.reduce((a, b) => a + b, 0) / scores.length;

    return (
      <View style={styles.chartContainer}>
        <Text style={styles.chartTitle}>Last 60 Minutes</Text>
        <Text style={styles.statRow}>Average: {avg.toFixed(0)}/100</Text>
        <Text style={styles.statRow}>Best: {maxScore}/100</Text>
        <Text style={styles.statRow}>Worst: {minScore}/100</Text>

        {/* Bar chart of recent scores */}
        <View style={styles.barChart}>
          {scores.slice(-30).map((score, i) => {
            const height = (score / 100) * 100;
            const color = score >= 70 ? '#10B981' : score >= 40 ? '#F59E0B' : '#EF4444';
            return (
              <View key={i} style={[styles.bar, { height: `${height}%`, backgroundColor: color }]} />
            );
          })}
        </View>

        <View style={styles.legend}>
          <View style={styles.legendItem}>
            <View style={[styles.legendDot, { backgroundColor: '#10B981' }]} />
            <Text style={styles.legendText}>Good (70-100)</Text>
          </View>
          <View style={styles.legendItem}>
            <View style={[styles.legendDot, { backgroundColor: '#F59E0B' }]} />
            <Text style={styles.legendText}>Fair (40-69)</Text>
          </View>
          <View style={styles.legendItem}>
            <View style={[styles.legendDot, { backgroundColor: '#EF4444' }]} />
            <Text style={styles.legendText}>Poor (0-39)</Text>
          </View>
        </View>
      </View>
    );
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Posture History</Text>

      {/* RSI Risk Card */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Cumulative RSI Risk</Text>
        <Text style={[styles.riskValue, {
          color: rsiRiskLevel === 'low' ? '#10B981' :
                 rsiRiskLevel === 'medium' ? '#F59E0B' : '#EF4444'
        }]}>
          {rsiRiskScore}% — {rsiRiskLevel.toUpperCase()}
        </Text>
      </View>

      {renderChart()}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#F3F4F6', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#111827', marginBottom: 16 },
  card: {
    backgroundColor: '#FFFFFF', borderRadius: 12, padding: 16,
    shadowColor: '#000', shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1, shadowRadius: 3, elevation: 2, marginBottom: 16,
  },
  cardTitle: { fontSize: 13, fontWeight: '600', color: '#6B7280', textTransform: 'uppercase', marginBottom: 8 },
  riskValue: { fontSize: 28, fontWeight: 'bold' },
  chartContainer: {
    backgroundColor: '#FFFFFF', borderRadius: 12, padding: 16,
    shadowColor: '#000', shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1, shadowRadius: 3, elevation: 2, marginBottom: 16,
  },
  chartTitle: { fontSize: 16, fontWeight: '600', color: '#111827', marginBottom: 12 },
  statRow: { fontSize: 14, color: '#374151', marginBottom: 4 },
  barChart: {
    flexDirection: 'row', alignItems: 'flex-end', height: 120,
    gap: 2, marginTop: 12, marginBottom: 12,
  },
  bar: { flex: 1, minHeight: 4, borderRadius: 2 },
  legend: { flexDirection: 'row', gap: 16, marginTop: 8 },
  legendItem: { flexDirection: 'row', alignItems: 'center', gap: 4 },
  legendDot: { width: 8, height: 8, borderRadius: 4 },
  legendText: { fontSize: 12, color: '#6B7280' },
  emptyText: { fontSize: 14, color: '#9CA3AF', textAlign: 'center', padding: 32 },
});