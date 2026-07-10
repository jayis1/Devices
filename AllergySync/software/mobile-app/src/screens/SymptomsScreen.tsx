/**
 * AllergySync — Symptoms Screen
 * Log and view symptoms with severity 0-5
 */

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Alert } from 'react-native';

const API_BASE = 'https://api.allergysync.io/api/v1';

const SYMPTOMS = [
  { key: 'sneezing', label: 'Sneezing', icon: '💨' },
  { key: 'itchy_eyes', label: 'Itchy Eyes', icon: '👁️' },
  { key: 'congestion', label: 'Congestion', icon: '👃' },
  { key: 'runny_nose', label: 'Runny Nose', icon: '💧' },
  { key: 'headache', label: 'Headache', icon: '🤕' },
  { key: 'fatigue', label: 'Fatigue', icon: '😴' },
];

export default function SymptomsScreen() {
  const [ratings, setRatings] = useState<Record<string, number>>({});
  const [history, setHistory] = useState<any[]>([]);

  const fetchHistory = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/symptoms?days=30`);
      const data = await res.json();
      setHistory(data.symptoms || []);
    } catch (err) {
      console.error('Fetch error:', err);
    }
  }, []);

  useEffect(() => { fetchHistory(); }, [fetchHistory]);

  const setRating = (key: string, value: number) => {
    setRatings(prev => ({ ...prev, [key]: value }));
  };

  const submit = async () => {
    const entry = {
      ...ratings,
      timestamp: new Date().toISOString(),
      notes: '',
    };
    try {
      await fetch(`${API_BASE}/symptoms`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(entry),
      });
      Alert.alert('Logged', 'Symptoms recorded successfully.');
      setRatings({});
      fetchHistory();
    } catch (err) {
      Alert.alert('Error', 'Failed to log symptoms.');
    }
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Log Symptoms</Text>
        {SYMPTOMS.map(s => (
          <View key={s.key} style={styles.symptomRow}>
            <Text style={styles.symptomLabel}>
              {s.icon} {s.label}
            </Text>
            <View style={styles.ratingRow}>
              {[0, 1, 2, 3, 4, 5].map(n => (
                <TouchableOpacity
                  key={n}
                  onPress={() => setRating(s.key, n)}
                  style={[
                    styles.ratingButton,
                    (ratings[s.key] ?? 0) === n && styles.ratingButtonActive,
                  ]}
                >
                  <Text style={[
                    styles.ratingText,
                    (ratings[s.key] ?? 0) === n && styles.ratingTextActive,
                  ]}>{n}</Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>
        ))}
        <TouchableOpacity style={styles.submitButton} onPress={submit}>
          <Text style={styles.submitButtonText}>Log Entry</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.card}>
        <Text style={styles.title}>Recent History</Text>
        {history.slice(0, 20).map((h, i) => (
          <View key={i} style={styles.historyRow}>
            <Text style={styles.historyDate}>
              {new Date(h.timestamp).toLocaleDateString()} {new Date(h.timestamp).toLocaleTimeString()}
            </Text>
            <Text style={styles.historySeverity}>
              Severity: {(h.total_severity ?? 0).toFixed(1)}/5
            </Text>
          </View>
        ))}
        {history.length === 0 && (
          <Text style={styles.emptyText}>No symptoms logged yet.</Text>
        )}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { backgroundColor: 'white', margin: 12, padding: 16, borderRadius: 12 },
  title: { fontSize: 18, fontWeight: 'bold', marginBottom: 12, color: '#333' },
  symptomRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#f0f0f0' },
  symptomLabel: { fontSize: 16, color: '#555' },
  ratingRow: { flexDirection: 'row', gap: 4 },
  ratingButton: { width: 32, height: 32, borderRadius: 16, borderWidth: 1, borderColor: '#ddd', justifyContent: 'center', alignItems: 'center' },
  ratingButtonActive: { backgroundColor: '#2E7D32', borderColor: '#2E7D32' },
  ratingText: { fontSize: 14, color: '#666' },
  ratingTextActive: { color: 'white', fontWeight: 'bold' },
  submitButton: { backgroundColor: '#2E7D32', padding: 16, borderRadius: 8, alignItems: 'center', marginTop: 16 },
  submitButtonText: { color: 'white', fontSize: 16, fontWeight: 'bold' },
  historyRow: { paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: '#f0f0f0' },
  historyDate: { fontSize: 12, color: '#999' },
  historySeverity: { fontSize: 14, color: '#333', fontWeight: '600' },
  emptyText: { color: '#999', textAlign: 'center', padding: 20 },
});