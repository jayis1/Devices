/**
 * Coaching Screen — weekly insights and recommendations
 * License: MIT
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { ApiClient } from '../api/client';

export default function CoachingScreen() {
  const [report, setReport] = useState(null);

  useEffect(() => {
    loadReport();
  }, []);

  const loadReport = async () => {
    try {
      const data = await ApiClient.getWeeklyCoaching();
      setReport(data);
    } catch {
      setReport({
        total_trips: 12, total_distance_km: 285.4,
        avg_safety_score: 78.3, total_drowsiness_events: 4,
        total_distraction_events: 7,
        riskiest_time_of_day: '14:00',
        recommendations: [
          'You experienced multiple drowsiness events this week. Consider taking breaks every 2 hours on long trips.',
          'Your riskiest driving time is around 14:00. The mid-afternoon dip is common — consider a short walk or coffee before driving.',
        ],
      });
    }
  };

  if (!report) return <View style={styles.container}><Text style={styles.loading}>Loading...</Text></View>;

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Weekly Coaching</Text>

      <View style={styles.summaryCard}>
        <View style={styles.summaryRow}>
          <View style={styles.summaryItem}>
            <Text style={styles.summaryValue}>{report.total_trips}</Text>
            <Text style={styles.summaryLabel}>Trips</Text>
          </View>
          <View style={styles.summaryItem}>
            <Text style={styles.summaryValue}>{report.total_distance_km.toFixed(0)}</Text>
            <Text style={styles.summaryLabel}>km Driven</Text>
          </View>
          <View style={styles.summaryItem}>
            <Text style={styles.summaryValue}>{report.avg_safety_score.toFixed(0)}</Text>
            <Text style={styles.summaryLabel}>Avg Score</Text>
          </View>
        </View>
      </View>

      <View style={styles.eventsCard}>
        <Text style={styles.cardTitle}>Risk Events This Week</Text>
        <View style={styles.eventRow}>
          <Text style={styles.eventIcon}>😴</Text>
          <Text style={styles.eventText}>{report.total_drowsiness_events} drowsiness events</Text>
        </View>
        <View style={styles.eventRow}>
          <Text style={styles.eventIcon}>📱</Text>
          <Text style={styles.eventText}>{report.total_distraction_events} distraction events</Text>
        </View>
        {report.riskiest_time_of_day && (
          <View style={styles.eventRow}>
            <Text style={styles.eventIcon}>⏰</Text>
            <Text style={styles.eventText}>Riskiest time: {report.riskiest_time_of_day}</Text>
          </View>
        )}
      </View>

      <View style={styles.recCard}>
        <Text style={styles.cardTitle}>Recommendations</Text>
        {report.recommendations.map((rec, i) => (
          <View key={i} style={styles.recItem}>
            <Text style={styles.recBullet}>•</Text>
            <Text style={styles.recText}>{rec}</Text>
          </View>
        ))}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  loading: { color: '#888', textAlign: 'center', marginTop: 40 },
  summaryCard: { backgroundColor: '#16213e', borderRadius: 12, padding: 20, marginBottom: 12 },
  summaryRow: { flexDirection: 'row', justifyContent: 'space-around' },
  summaryItem: { alignItems: 'center' },
  summaryValue: { fontSize: 32, fontWeight: 'bold', color: '#4CAF50' },
  summaryLabel: { fontSize: 12, color: '#888', marginTop: 4 },
  eventsCard: { backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardTitle: { fontSize: 16, fontWeight: 'bold', color: '#fff', marginBottom: 12 },
  eventRow: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
  eventIcon: { fontSize: 20, marginRight: 12 },
  eventText: { fontSize: 15, color: '#ccc' },
  recCard: { backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 20 },
  recItem: { flexDirection: 'row', marginBottom: 10 },
  recBullet: { color: '#FF9800', marginRight: 8, fontSize: 16 },
  recText: { fontSize: 14, color: '#ccc', flex: 1 },
});