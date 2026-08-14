/** Coaching Screen — personalized exercises + ergonomic tips */
import React, { useState, useEffect, useContext } from 'react';
import { View, Text, StyleSheet, ScrollView, ActivityIndicator } from 'react-native';
import { PostureSyncContext } from '../services/PostureSyncContext';

export default function CoachingScreen() {
  const { api } = useContext(PostureSyncContext);
  const [coaching, setCoaching] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    (async () => {
      try {
        const data = await api.getCoaching();
        setCoaching(data);
      } catch (e) {
        console.error(e);
      } finally {
        setLoading(false);
      }
    })();
  }, []);

  if (loading) return <ActivityIndicator style={styles.loader} size="large" />;

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Ergonomic Coaching</Text>

      <Text style={styles.sectionTitle}>Recommended Exercises</Text>
      {coaching?.recommendations?.map((ex: any, i: number) => (
        <View key={i} style={styles.exerciseCard}>
          <Text style={styles.exerciseTitle}>{ex.title}</Text>
          <Text style={styles.exerciseDesc}>{ex.description}</Text>
          <Text style={styles.exerciseReason}>Why: {ex.reason}</Text>
          <Text style={styles.exerciseFreq}>Frequency: {ex.frequency}</Text>
        </View>
      ))}

      <Text style={styles.sectionTitle}>Ergonomic Tips</Text>
      {coaching?.ergonomic_tips?.map((tip: string, i: number) => (
        <Text key={i} style={styles.tip}>• {tip}</Text>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, backgroundColor: '#FAFAFA' },
  loader: { flex: 1, justifyContent: 'center' },
  title: { fontSize: 24, fontWeight: '700', marginBottom: 16, color: '#212121' },
  sectionTitle: { fontSize: 18, fontWeight: '600', marginTop: 16, marginBottom: 12, color: '#212121' },
  exerciseCard: { padding: 16, backgroundColor: '#FFF', borderRadius: 12, marginBottom: 12, elevation: 2 },
  exerciseTitle: { fontSize: 16, fontWeight: '700', marginBottom: 4, color: '#2196F3' },
  exerciseDesc: { fontSize: 14, color: '#424242', marginBottom: 4 },
  exerciseReason: { fontSize: 12, color: '#757575', marginBottom: 2 },
  exerciseFreq: { fontSize: 12, color: '#757575' },
  tip: { fontSize: 14, color: '#424242', marginBottom: 6, paddingLeft: 8 },
});