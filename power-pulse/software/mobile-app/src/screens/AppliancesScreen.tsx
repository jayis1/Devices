/**
 * PowerPulse — Appliances Screen (React Native)
 */

import React, { useState } from 'react';
import { View, Text, ScrollView, StyleSheet, Switch, TouchableOpacity, RefreshControl } from 'react-native';

const API_BASE = 'http://powerpulse.local:8000/api/v1';

interface Appliance {
  id: number;
  name: string;
  watts: number;
  relay_on: boolean;
}

const MOCK_APPLIANCES: Appliance[] = [
  { id: 1, name: 'Refrigerator', watts: 150, relay_on: true },
  { id: 2, name: 'TV & Entertainment', watts: 85, relay_on: true },
  { id: 3, name: 'Washing Machine', watts: 0, relay_on: false },
  { id: 5, name: 'Desk Lamp', watts: 12, relay_on: true },
  { id: 7, name: 'Space Heater', watts: 1200, relay_on: true },
];

export default function AppliancesScreen() {
  const [appliances, setAppliances] = useState(MOCK_APPLIANCES);
  
  const toggleRelay = async (id: number, currentState: boolean) => {
    try {
      // In production: await fetch(`${API_BASE}/devices/${id}/command`, { ... })
      setAppliances(prev => prev.map(a => 
        a.id === id ? { ...a, relay_on: !currentState } : a
      ));
    } catch (error) {
      console.error('Failed to toggle relay:', error);
    }
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Appliances</Text>
      <Text style={styles.subtitle}>{appliances.length} tagged devices</Text>
      
      {appliances.map(appliance => (
        <View key={appliance.id} style={styles.card}>
          <View style={styles.cardHeader}>
            <Text style={styles.appName}>{appliance.name}</Text>
            <Switch
              value={appliance.relay_on}
              onValueChange={() => toggleRelay(appliance.id, appliance.relay_on)}
              trackColor={{ false: '#333', true: '#00d4aa' }}
              thumbColor="#fff"
            />
          </View>
          <View style={styles.cardBody}>
            <Text style={styles.watts}>{appliance.watts} W</Text>
            <Text style={[styles.status, { color: appliance.relay_on ? '#00d4aa' : '#666' }]}>
              {appliance.relay_on ? 'ON' : 'OFF'}
            </Text>
          </View>
        </View>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0a0a23', padding: 16 },
  title: { color: '#fff', fontSize: 24, fontWeight: '700' },
  subtitle: { color: '#888', fontSize: 14, marginTop: 4, marginBottom: 16 },
  card: { backgroundColor: '#16213e', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardHeader: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  appName: { color: '#fff', fontSize: 16, fontWeight: '600' },
  cardBody: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginTop: 8 },
  watts: { color: '#ffd700', fontSize: 24, fontWeight: '700' },
  status: { fontSize: 14, fontWeight: '600' },
});