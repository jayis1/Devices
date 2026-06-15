/**
 * ErgoFlow — Dashboard Screen
 * Main screen showing live posture score, activity, and environment
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import React, { useEffect, useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  RefreshControl,
} from 'react-native';
import useErgoFlowStore from '../state/ErgoFlowContext';

const POSTURE_COLORS: Record<string, string> = {
  good: '#10B981',
  slouch: '#F59E0B',
  lean_left: '#F97316',
  lean_right: '#F97316',
  hunch: '#EF4444',
};

const POSTURE_ICONS: Record<string, string> = {
  good: '😊',
  slouch: '😔',
  lean_left: '↗️',
  lean_right: '↖️',
  hunch: '😣',
};

const ACTIVITY_ICONS: Record<string, string> = {
  typing: '⌨️',
  mouse: '🖱️',
  phone: '📱',
  idle: '💤',
  stretch: '🧘',
  walk: '🚶',
};

export default function DashboardScreen() {
  const [refreshing, setRefreshing] = useState(false);

  const {
    currentPosture,
    rsiRiskScore,
    currentActivity,
    activityConfidence,
    focusLevel,
    focusScore,
    environment,
    deskStatus,
    breakInfo,
    fetchPostureCurrent,
    fetchRsiRisk,
    fetchActivity,
    fetchFocus,
    fetchEnvironment,
    fetchDeskStatus,
    fetchBreaks,
  } = useErgoFlowStore();

  useEffect(() => {
    const interval = setInterval(() => {
      fetchPostureCurrent();
      fetchRsiRisk();
      fetchActivity();
      fetchFocus();
      fetchEnvironment();
      fetchDeskStatus();
      fetchBreaks();
    }, 2000);

    // Initial fetch
    fetchPostureCurrent();
    fetchRsiRisk();
    fetchActivity();
    fetchFocus();
    fetchEnvironment();
    fetchDeskStatus();
    fetchBreaks();

    return () => clearInterval(interval);
  }, []);

  const onRefresh = async () => {
    setRefreshing(true);
    await Promise.all([
      fetchPostureCurrent(),
      fetchRsiRisk(),
      fetchActivity(),
      fetchFocus(),
      fetchEnvironment(),
      fetchDeskStatus(),
      fetchBreaks(),
    ]);
    setRefreshing(false);
  };

  const score = currentPosture?.score ?? 0;
  const postureClass = currentPosture?.posture_class ?? 'unknown';
  const riskColor = POSTURE_COLORS[postureClass] || '#9CA3AF';

  // Circular gauge progress
  const circumference = 2 * Math.PI * 80;
  const progress = (score / 100) * circumference;

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Posture Score Gauge */}
      <View style={styles.scoreContainer}>
        <View style={[styles.scoreCircle, { borderColor: riskColor }]}>
          <Text style={[styles.scoreValue, { color: riskColor }]}>{score}</Text>
          <Text style={styles.scoreLabel}>Posture Score</Text>
        </View>
        <Text style={styles.postureClass}>
          {POSTURE_ICONS[postureClass] || '❓'} {postureClass.replace('_', ' ').toUpperCase()}
        </Text>
      </View>

      {/* Risk & Activity Row */}
      <View style={styles.row}>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>RSI Risk</Text>
          <Text style={[styles.cardValue, { color: rsiRiskScore > 60 ? '#EF4444' : rsiRiskScore > 30 ? '#F59E0B' : '#10B981' }]}>
            {rsiRiskScore}%
          </Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Activity</Text>
          <Text style={styles.cardValue}>
            {ACTIVITY_ICONS[currentActivity] || '❓'} {currentActivity}
          </Text>
          <Text style={styles.cardSub}>{activityConfidence}% confident</Text>
        </View>
      </View>

      {/* Focus & Environment Row */}
      <View style={styles.row}>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Focus</Text>
          <Text style={styles.cardValue}>{focusLevel.toUpperCase()}</Text>
          <Text style={styles.cardSub}>Score: {focusScore}</Text>
        </View>
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Environment</Text>
          <Text style={styles.cardSmall}>🌡️ {environment.temperature_c}°C</Text>
          <Text style={styles.cardSmall}>💧 {environment.humidity_pct}%</Text>
          <Text style={styles.cardSmall}>☀️ {environment.lux} lux</Text>
        </View>
      </View>

      {/* Desk Status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Desk</Text>
        <Text style={styles.cardValue}>{deskStatus.height_mm}mm</Text>
        <Text style={styles.cardSub}>{deskStatus.motor_state}</Text>
      </View>

      {/* Break Timer */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Next Break</Text>
        <Text style={styles.cardValue}>in {breakInfo.next_break_minutes} min</Text>
        <Text style={styles.cardSub}>
          {breakInfo.breaks_completed}/{breakInfo.breaks_today} completed today
        </Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F3F4F6',
    padding: 16,
  },
  scoreContainer: {
    alignItems: 'center',
    marginBottom: 24,
  },
  scoreCircle: {
    width: 180,
    height: 180,
    borderRadius: 90,
    borderWidth: 8,
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: '#FFFFFF',
  },
  scoreValue: {
    fontSize: 56,
    fontWeight: 'bold',
  },
  scoreLabel: {
    fontSize: 14,
    color: '#6B7280',
    marginTop: 4,
  },
  postureClass: {
    fontSize: 18,
    fontWeight: '600',
    marginTop: 12,
    color: '#374151',
  },
  row: {
    flexDirection: 'row',
    gap: 12,
    marginBottom: 12,
  },
  card: {
    flex: 1,
    backgroundColor: '#FFFFFF',
    borderRadius: 12,
    padding: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  cardTitle: {
    fontSize: 13,
    fontWeight: '600',
    color: '#6B7280',
    textTransform: 'uppercase',
    marginBottom: 8,
  },
  cardValue: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#111827',
  },
  cardSub: {
    fontSize: 12,
    color: '#9CA3AF',
    marginTop: 4,
  },
  cardSmall: {
    fontSize: 14,
    color: '#374151',
    marginBottom: 2,
  },
});