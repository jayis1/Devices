/**
 * AsthmaSync — Medication Screen
 * Adherence calendar, dose log, refill reminders.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, TouchableOpacity } from 'react-native';
import { ApiClient } from '../services/api';

interface AdherenceData {
  rescue_count_7d: number;
  rescue_count_30d: number;
  controller_adherence_pct: number;
  gina_controlled: boolean;
  last_rescue: string | null;
}

interface EventItem {
  timestamp: string;
  event_type: string;
  severity: number;
  message: string;
}

export default function MedicationScreen() {
  const [adherence, setAdherence] = useState<AdherenceData | null>(null);
  const [events, setEvents] = useState<EventItem[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    const api = ApiClient.getInstance();
    try {
      const [adherenceData, eventsData] = await Promise.all([
        api.getAdherence(),
        api.getEvents(30),
      ]);
      setAdherence(adherenceData);
      setEvents(eventsData.filter(e => e.event_type === 'actuation' || e.event_type === 'alert'));
    } catch (e) {
      console.error('Medication fetch error:', e);
    }
  }, []);

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  }, [fetchData]);

  // Simple 30-day calendar (current month)
  const today = new Date();
  const daysInMonth = new Date(today.getFullYear(), today.getMonth() + 1, 0).getDate();
  const calendarDays = Array.from({ length: daysInMonth }, (_, i) => i + 1);
  const currentDay = today.getDate();

  // Count actuations per day
  const actuationsByDay: Record<number, number> = {};
  events.forEach(e => {
    if (e.event_type === 'actuation') {
      const day = new Date(e.timestamp).getDate();
      actuationsByDay[day] = (actuationsByDay[day] || 0) + 1;
    }
  });

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Summary Card */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Medication Summary</Text>
        <View style={styles.summaryRow}>
          <View style={styles.summaryItem}>
            <Text style={styles.summaryValue}>
              {adherence?.rescue_count_7d ?? '--'}
            </Text>
            <Text style={styles.summaryLabel}>Rescue (7d)</Text>
          </View>
          <View style={styles.summaryItem}>
            <Text style={styles.summaryValue}>
              {adherence?.rescue_count_30d ?? '--'}
            </Text>
            <Text style={styles.summaryLabel}>Rescue (30d)</Text>
          </View>
          <View style={styles.summaryItem}>
            <Text style={[
              styles.summaryValue,
              { color: (adherence?.controller_adherence_pct || 0) > 80 ? '#43A047' : '#FB8C00' }
            ]}>
              {Math.round(adherence?.controller_adherence_pct || 0)}%
            </Text>
            <Text style={styles.summaryLabel}>Controller</Text>
          </View>
        </View>
      </View>

      {/* Calendar */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>
          {today.toLocaleDateString('en-US', { month: 'long', year: 'numeric' })}
        </Text>
        <View style={styles.calendarGrid}>
          {calendarDays.map(day => {
            const count = actuationsByDay[day] || 0;
            const isToday = day === currentDay;
            const bgColor = count === 0 ? '#f0f0f0' :
                           count <= 2 ? '#FFE0B2' :
                           count <= 4 ? '#FFB74D' : '#E53935';
            return (
              <View
                key={day}
                style={[
                  styles.calendarDay,
                  { backgroundColor: bgColor },
                  isToday && styles.calendarToday,
                ]}
              >
                <Text style={styles.calendarDayText}>{day}</Text>
                {count > 0 && (
                  <Text style={styles.calendarCount}>{count}</Text>
                )}
              </View>
            );
          })}
        </View>
        <Text style={styles.calendarLegend}>
          🟡 1-2 uses  🟠 3-4 uses  🔴 5+ uses
        </Text>
      </View>

      {/* Recent Events */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Recent Doses & Alerts</Text>
        {events.length === 0 ? (
          <Text style={styles.emptyText}>No recent events</Text>
        ) : (
          events.map((e, i) => (
            <View key={i} style={styles.eventRow}>
              <View style={[styles.eventDot,
                { backgroundColor: e.severity >= 2 ? '#E53935' : '#FB8C00' }]} />
              <View style={styles.eventContent}>
                <Text style={styles.eventMessage}>{e.message}</Text>
                <Text style={styles.eventTime}>
                  {new Date(e.timestamp).toLocaleString()}
                </Text>
              </View>
            </View>
          ))
        )}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f0f4f8' },
  card: { backgroundColor: '#fff', borderRadius: 12, padding: 16, margin: 12 },
  cardTitle: { fontSize: 18, fontWeight: 'bold', color: '#333', marginBottom: 12 },
  summaryRow: { flexDirection: 'row', justifyContent: 'space-around' },
  summaryItem: { alignItems: 'center' },
  summaryValue: { fontSize: 32, fontWeight: 'bold', color: '#0066CC' },
  summaryLabel: { fontSize: 12, color: '#888', marginTop: 4 },
  calendarGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  calendarDay: {
    width: 38, height: 44, borderRadius: 6, margin: 3,
    alignItems: 'center', justifyContent: 'center',
  },
  calendarToday: { borderWidth: 2, borderColor: '#0066CC' },
  calendarDayText: { fontSize: 12, fontWeight: '600', color: '#333' },
  calendarCount: { fontSize: 10, color: '#666', marginTop: 2 },
  calendarLegend: { fontSize: 11, color: '#888', marginTop: 12, textAlign: 'center' },
  eventRow: { flexDirection: 'row', alignItems: 'center', marginVertical: 8 },
  eventDot: { width: 8, height: 8, borderRadius: 4, marginRight: 12 },
  eventContent: { flex: 1 },
  eventMessage: { fontSize: 14, color: '#333' },
  eventTime: { fontSize: 11, color: '#999', marginTop: 2 },
  emptyText: { fontSize: 14, color: '#999', textAlign: 'center', padding: 20 },
});