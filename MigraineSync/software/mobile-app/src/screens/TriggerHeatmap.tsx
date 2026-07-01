/**
 * MigraineSync — Trigger Heatmap Screen
 * ======================================
 * Shows which triggers correlate with the user's migraine attacks.
 *
 * License: MIT
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { getTriggers, TriggerAttribution } from '../services/api';

export default function TriggerHeatmap() {
  const [triggers, setTriggers] = useState<TriggerAttribution[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchTriggers = useCallback(async () => {
    try {
      const data = await getTriggers();
      setTriggers(data);
    } catch (e) {
      console.error('Failed to fetch triggers:', e);
    }
  }, []);

  useEffect(() => {
    fetchTriggers();
  }, [fetchTriggers]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchTriggers();
    setRefreshing(false);
  }, [fetchTriggers]);

  const triggerIcons: Record<string, string> = {
    barometric_pressure: 'cloud',
    stress: 'flash',
    sleep_quality: 'moon',
    hydration: 'water',
    light_exposure: 'sunny',
    noise: 'volume-high',
  };

  const exposureColors: Record<string, string> = {
    high: '#E74C3C',
    moderate: '#F39C12',
    low: '#27AE60',
    adequate: '#27AE60',
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      <View style={styles.header}>
        <Text style={styles.title}>Your Trigger Profile</Text>
        <Text style={styles.subtitle}>Personalized SHAP attribution (7-day window)</Text>
      </View>

      {triggers.map((t, i) => (
        <View key={i} style={styles.triggerCard}>
          <View style={styles.triggerHeader}>
            <Ionicons
              name={(triggerIcons[t.trigger] || 'alert-circle') as any}
              size={28}
              color={exposureColors[t.exposure_level] || '#6C5CE7'}
            />
            <View style={styles.triggerInfo}>
              <Text style={styles.triggerName}>
                {t.trigger.replace(/_/g, ' ').toUpperCase()}
              </Text>
              <Text style={[styles.exposureLevel, { color: exposureColors[t.exposure_level] || '#636E72' }]}>
                {t.exposure_level.toUpperCase()}
              </Text>
            </View>
            <Text style={styles.contribution}>{t.contribution_pct.toFixed(1)}%</Text>
          </View>

          {/* Contribution bar */}
          <View style={styles.barContainer}>
            <View
              style={[
                styles.bar,
                {
                  width: `${Math.min(100, t.contribution_pct * 2)}%`,
                  backgroundColor: exposureColors[t.exposure_level] || '#6C5CE7',
                },
              ]}
            />
          </View>

          <Text style={styles.recommendation}>{t.recommendation}</Text>
        </View>
      ))}

      <Text style={styles.disclaimer}>
        Trigger attribution is based on XGBoost SHAP analysis of your personal data.
        Results improve over time as more data is collected.
      </Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1A1A2E' },
  header: { padding: 20, paddingTop: 50 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#6C5CE7' },
  subtitle: { fontSize: 12, color: '#636E72', marginTop: 4 },
  triggerCard: { margin: 16, padding: 16, borderRadius: 12, backgroundColor: '#16213E' },
  triggerHeader: { flexDirection: 'row', alignItems: 'center' },
  triggerInfo: { flex: 1, marginLeft: 12 },
  triggerName: { fontSize: 14, fontWeight: '600', color: '#FFFFFF' },
  exposureLevel: { fontSize: 11, marginTop: 2 },
  contribution: { fontSize: 22, fontWeight: 'bold', color: '#6C5CE7' },
  barContainer: { height: 8, backgroundColor: '#0D1117', borderRadius: 4, marginTop: 12 },
  bar: { height: 8, borderRadius: 4 },
  recommendation: { fontSize: 12, color: '#A0A0B0', marginTop: 12, flexWrap: 'wrap' },
  disclaimer: { padding: 20, fontSize: 10, color: '#636E72', textAlign: 'center' },
});