/**
 * SightSync Mobile App — Myopia Tracking Screen (for children)
 * License: MIT
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { Card, ProgressBar, Button } from 'react-native-paper';
import api from '../api/client';

export default function MyopiaTrackingScreen() {
  const [forecast, setForecast] = useState({
    risk_30day: 0,
    risk_90day: 0,
    refractive_delta_diopter: 0,
    near_work_today_min: 0,
    outdoor_today_min: 0,
    avg_distance_mm: 0,
    recommendation: '',
  });

  useEffect(() => {
    api.getMyopiaForecast().then((res) => setForecast(res.data)).catch(console.error);
  }, []);

  const riskColor = (risk: number) => {
    if (risk >= 70) return '#D32F2F';
    if (risk >= 40) return '#F57C00';
    if (risk >= 20) return '#FBC02D';
    return '#43A047';
  };

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Title title="Myopia Progression Risk" subtitle="90-day forecast for your child" />
        <Card.Content>
          <View style={styles.riskRow}>
            <View style={styles.riskItem}>
              <Text style={[styles.riskValue, { color: riskColor(forecast.risk_30day) }]}>
                {forecast.risk_30day}%
              </Text>
              <Text style={styles.riskLabel}>30-day risk</Text>
            </View>
            <View style={styles.riskItem}>
              <Text style={[styles.riskValue, { color: riskColor(forecast.risk_90day) }]}>
                {forecast.risk_90day}%
              </Text>
              <Text style={styles.riskLabel}>90-day risk</Text>
            </View>
          </View>
          <Text style={styles.refractionText}>
            Projected change: {forecast.refractive_delta_diopter.toFixed(2)} D
          </Text>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Title title="Today's Exposure" />
        <Card.Content>
          <Text style={styles.exposureText}>
            📖 Near-work: {forecast.near_work_today_min} min
          </Text>
          <ProgressBar
            progress={Math.min(forecast.near_work_today_min / 180, 1)}
            color={forecast.near_work_today_min > 120 ? '#F57C00' : '#43A047'}
            style={styles.progressBar}
          />

          <Text style={styles.exposureText}>
            ☀️ Outdoor light: {forecast.outdoor_today_min} min
          </Text>
          <ProgressBar
            progress={Math.min(forecast.outdoor_today_min / 120, 1)}
            color={forecast.outdoor_today_min < 120 ? '#F57C00' : '#43A047'}
            style={styles.progressBar}
          />

          <Text style={styles.exposureText}>
            📏 Avg distance: {forecast.avg_distance_mm} mm
          </Text>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Title title="Recommendations" />
        <Card.Content>
          <Text style={styles.recommendation}>{forecast.recommendation || 'Keep up the good work!'}</Text>
        </Card.Content>
      </Card>

      <Button mode="contained" style={styles.button} buttonColor="#0066CC"
              onPress={() => api.getOptometristReport('pdf')}>
        Export Optometrist Report
      </Button>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 8 },
  card: { marginVertical: 8, elevation: 2 },
  riskRow: { flexDirection: 'row', justifyContent: 'space-around', marginVertical: 16 },
  riskItem: { alignItems: 'center' },
  riskValue: { fontSize: 48, fontWeight: 'bold' },
  riskLabel: { fontSize: 14, color: '#888' },
  refractionText: { fontSize: 16, textAlign: 'center', color: '#666' },
  exposureText: { fontSize: 16, marginVertical: 8 },
  progressBar: { height: 6, borderRadius: 3, marginVertical: 4 },
  recommendation: { fontSize: 14, color: '#333' },
  button: { marginVertical: 16 },
});