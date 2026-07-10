/**
 * AllergySync — Insights Screen
 * Weekly/monthly exposure vs symptom correlation, personalized tips
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, Picker } from 'react-native';

const API_BASE = 'https://api.allergysync.io/api/v1';

export default function InsightsScreen() {
  const [period, setPeriod] = useState('weekly');
  const [insights, setInsights] = useState<any>(null);

  const fetchInsights = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/insights?period=${period}`);
      const data = await res.json();
      setInsights(data);
    } catch (err) {
      console.error('Fetch error:', err);
    }
  }, [period]);

  useEffect(() => { fetchInsights(); }, [fetchInsights]);

  return (
    <ScrollView style={styles.container}>
      <View style={styles.card}>
        <View style={styles.headerRow}>
          <Text style={styles.title}>Insights</Text>
          <Picker
            selectedValue={period}
            style={{ width: 120, height: 40 }}
            onValueChange={setPeriod}
          >
            <Picker.Item label="Weekly" value="weekly" />
            <Picker.Item label="Monthly" value="monthly" />
          </Picker>
        </View>

        <View style={styles.statRow}>
          <View style={styles.stat}>
            <Text style={styles.statValue}>
              {insights?.avg_symptom_severity?.toFixed(1) ?? '—'}
            </Text>
            <Text style={styles.statLabel}>Avg Severity</Text>
          </View>
          <View style={styles.stat}>
            <Text style={styles.statValue}>
              {insights?.symptom_entries ?? '—'}
            </Text>
            <Text style={styles.statLabel}>Entries</Text>
          </View>
        </View>

        {insights?.medication_doses?.map((m: any, i: number) => (
          <View key={i} style={styles.medRow}>
            <Text style={styles.medName}>{m.medication}</Text>
            <Text style={styles.medDoses}>{m.doses} doses</Text>
          </View>
        ))}
      </View>

      <View style={styles.card}>
        <Text style={styles.title}>Personalized Tip</Text>
        <Text style={styles.tipText}>
          {insights?.tip || 'Complete your allergy profile for personalized insights.'}
        </Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { backgroundColor: 'white', margin: 12, padding: 16, borderRadius: 12 },
  headerRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  title: { fontSize: 18, fontWeight: 'bold', color: '#333' },
  statRow: { flexDirection: 'row', justifyContent: 'space-around', marginVertical: 16 },
  stat: { alignItems: 'center' },
  statValue: { fontSize: 28, fontWeight: 'bold', color: '#2E7D32' },
  statLabel: { fontSize: 12, color: '#999', marginTop: 4 },
  medRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#f0f0f0' },
  medName: { fontSize: 14, color: '#555' },
  medDoses: { fontSize: 14, color: '#333', fontWeight: '600' },
  tipText: { fontSize: 15, color: '#555', lineHeight: 22, marginTop: 8 },
});