/**
 * PowerPulse — Bill Projection Card Component
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet } from 'react-native';

const API_BASE = 'http://powerpulse.local:8000/api/v1';

export function BillProjectionCard() {
  const [estimate, setEstimate] = useState<{ total_kwh: number; estimated_cost: number } | null>(null);
  
  useEffect(() => {
    const fetchEstimate = async () => {
      try {
        const response = await fetch(`${API_BASE}/billing/estimate`);
        if (response.ok) {
          const data = await response.json();
          setEstimate(data);
        }
      } catch {}
    };
    fetchEstimate();
    const interval = setInterval(fetchEstimate, 60000);
    return () => clearInterval(interval);
  }, []);
  
  if (!estimate) return null;
  
  return (
    <View style={styles.card}>
      <Text style={styles.title}>Bill Projection</Text>
      <View style={styles.row}>
        <View style={styles.item}>
          <Text style={styles.label}>This Month</Text>
          <Text style={styles.value}>${estimate.estimated_cost.toFixed(2)}</Text>
        </View>
        <View style={styles.item}>
          <Text style={styles.label}>Usage</Text>
          <Text style={styles.value}>{estimate.total_kwh.toFixed(1)} kWh</Text>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  card: { backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 16 },
  title: { color: '#fff', fontSize: 18, fontWeight: '700', marginBottom: 12 },
  row: { flexDirection: 'row', justifyContent: 'space-between' },
  item: { alignItems: 'center' },
  label: { color: '#888', fontSize: 12 },
  value: { color: '#fff', fontSize: 20, fontWeight: '700' },
});