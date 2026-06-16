/**
 * PowerPulse — Solar Screen (React Native)
 */

import React from 'react';
import { View, Text, ScrollView, StyleSheet } from 'react-native';
import { useSolar } from '../hooks/useSolar';

export default function SolarScreen() {
  const { solar, loading } = useSolar();
  
  const formatWatts = (w: number) => w >= 1000 ? `${(w/1000).toFixed(1)} kW` : `${w} W`;
  
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Solar & Battery</Text>
      
      {/* Solar Production */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>☀ Solar Production</Text>
        <Text style={styles.bigNumber}>{formatWatts(solar?.pv_power_w || 0)}</Text>
        <Text style={styles.detail}>Voltage: {(solar?.pv_voltage_mv || 0) / 1000}V  Current: {(solar?.pv_current_ma || 0) / 1000}A</Text>
      </View>

      {/* Battery Status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>🔋 Battery</Text>
        <Text style={styles.bigNumber}>{solar?.soc_pct || 0}%</Text>
        <Text style={styles.detail}>
          Voltage: {(solar?.batt_voltage_mv || 0) / 1000}V  Mode: {
            solar?.charge_mode === 1 ? 'Bulk' :
            solar?.charge_mode === 2 ? 'Absorption' :
            solar?.charge_mode === 3 ? 'Float' : 'Standby'
          }
        </Text>
      </View>

      {/* Load */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>🏠 Home Load</Text>
        <Text style={styles.bigNumber}>{formatWatts(solar?.load_power_w || 0)}</Text>
      </View>

      {/* MPPT Status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>⚙ MPPT Controller</Text>
        <Text style={styles.detail}>Duty Cycle: {solar?.mppt_duty_pct || 0}%</Text>
        <Text style={styles.detail}>Heatsink: {solar?.heatsink_temp_c || 0}°C</Text>
        <Text style={styles.detail}>Fan: {solar?.fan_speed_pct || 0}%</Text>
      </View>

      {/* Energy Totals */}
      <View style={styles.row}>
        <View style={[styles.card, styles.halfCard]}>
          <Text style={styles.cardTitleSmall}>Produced</Text>
          <Text style={styles.mediumNumber}>{solar?.energy_produced_wh || 0} Wh</Text>
        </View>
        <View style={[styles.card, styles.halfCard]}>
          <Text style={styles.cardTitleSmall}>Consumed</Text>
          <Text style={styles.mediumNumber}>{solar?.energy_consumed_wh || 0} Wh</Text>
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a0a23', padding: 16 },
  title: { color: '#fff', fontSize: 24, fontWeight: '700', marginBottom: 16 },
  card: { backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardTitle: { color: '#888', fontSize: 14, fontWeight: '600', marginBottom: 4 },
  cardTitleSmall: { color: '#888', fontSize: 12, fontWeight: '600' },
  bigNumber: { color: '#ffd700', fontSize: 32, fontWeight: '700' },
  mediumNumber: { color: '#fff', fontSize: 20, fontWeight: '600' },
  detail: { color: '#aaa', fontSize: 13, marginTop: 2 },
  row: { flexDirection: 'row', gap: 12 },
  halfCard: { flex: 1 },
});