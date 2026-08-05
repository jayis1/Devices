// SeizureSync — Dashboard screen (main overview)
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { LineChart } from 'react-native-chart-kit';
import axios from 'axios';

const API_BASE = 'https://api.seizuresync.com';

export default function DashboardScreen() {
  const [risk24h, setRisk24h] = useState(0);
  const [lastEvent, setLastEvent] = useState<string | null>(null);
  const [sudepRisk, setSudepRisk] = useState(0);
  const [bandStatus, setBandStatus] = useState('Connected');

  useEffect(() => {
    // Fetch patient status
    axios.get(`${API_BASE}/patients/me/risk`).then(r => setRisk24h(r.data.risk_24h));
    axios.get(`${API_BASE}/patients/me/sudep`).then(r => setSudepRisk(r.data.annual_risk_pct));
  }, []);

  const riskColor = risk24h > 60 ? '#FF4444' : risk24h > 30 ? '#FFAA00' : '#00AA00';

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>SeizureSync</Text>
        <Text style={styles.subtitle}>Epilepsy Monitoring</Text>
      </View>

      {/* 24-hour risk gauge */}
      <View style={[styles.card, { borderLeftColor: riskColor }]}>
        <Text style={styles.cardTitle}>24-Hour Seizure Risk</Text>
        <Text style={[styles.riskValue, { color: riskColor }]}>{risk24h.toFixed(1)}%</Text>
        <Text style={styles.cardSub}>Model: RiskNet v1</Text>
      </View>

      {/* Band status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Seizure Band</Text>
        <Text style={styles.statusText}>● {bandStatus}</Text>
        <Text style={styles.cardSub}>Battery: 85% · Signal: OK</Text>
      </View>

      {/* Aura patch status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Aura Patch</Text>
        <Text style={styles.statusText}>● Worn (Day 3/14)</Text>
        <Text style={styles.cardSub}>Battery: 72%</Text>
      </View>

      {/* SUDEP risk */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>SUDEP Risk (Annual)</Text>
        <Text style={[styles.riskValue, { fontSize: 28 }]}>{sudepRisk.toFixed(2)}%</Text>
        <Text style={styles.cardSub}>Last assessed: Today</Text>
      </View>

      {/* Last event */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>Last Seizure Event</Text>
        <Text style={styles.statusText}>{lastEvent || 'No events recorded'}</Text>
        <Text style={styles.cardSub}>Tap "Diary" for full history</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { padding: 20, backgroundColor: '#0A1144' },
  title: { fontSize: 28, fontWeight: 'bold', color: '#fff' },
  subtitle: { fontSize: 14, color: '#aaa' },
  card: { margin: 10, padding: 16, backgroundColor: '#fff',
         borderRadius: 12, borderLeftWidth: 4, borderLeftColor: '#0A1144',
         shadowColor: '#000', shadowOpacity: 0.1, shadowRadius: 4,
         elevation: 2 },
  cardTitle: { fontSize: 14, color: '#666', marginBottom: 8 },
  riskValue: { fontSize: 36, fontWeight: 'bold' },
  cardSub: { fontSize: 12, color: '#999', marginTop: 4 },
  statusText: { fontSize: 18, fontWeight: '600', color: '#333' },
});