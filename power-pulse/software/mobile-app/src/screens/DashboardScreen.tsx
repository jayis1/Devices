/**
 * PowerPulse — Dashboard Screen (React Native)
 * 
 * Real-time power flow visualization showing total consumption,
 * solar production, grid import/export, and per-circuit breakdown.
 */

import React, { useEffect, useState } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl } from 'react-native';
import { useEnergy } from '../hooks/useEnergy';
import { useAlerts } from '../hooks/useAlerts';
import { PowerFlowCard } from '../components/PowerFlowCard';
import { CircuitGauge } from '../components/CircuitGauge';
import { AlertBadge } from '../components/AlertBadge';
import { BillProjectionCard } from '../components/BillProjectionCard';

const API_BASE = 'http://powerpulse.local:8000/api/v1';

export default function DashboardScreen() {
  const { powerFlow, loading: energyLoading, refresh: energyRefresh } = useEnergy();
  const { alerts, unreadCount } = useAlerts();
  const [refreshing, setRefreshing] = useState(false);

  const onRefresh = async () => {
    setRefreshing(true);
    await energyRefresh();
    setRefreshing(false);
  };

  // Real-time updates via WebSocket
  useEffect(() => {
    const ws = new WebSocket(`ws://powerpulse.local:8000/ws/realtime`);
    
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      // Update state with real-time data
    };
    
    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };
    
    return () => ws.close();
  }, []);

  return (
    <ScrollView 
      style={styles.container}
      refreshControl={
        <RefreshControl refreshing={refreshing} onRefresh={onRefresh} />
      }
    >
      {/* Alert Banner */}
      {unreadCount > 0 && (
        <AlertBadge count={unreadCount} severity={alerts[0]?.severity || 1} />
      )}

      {/* Real-time Power Flow */}
      <PowerFlowCard
        totalConsumption={powerFlow?.total_consumption_w || 0}
        solarProduction={powerFlow?.solar_production_w || 0}
        gridImport={powerFlow?.grid_import_w || 0}
        gridExport={powerFlow?.grid_export_w || 0}
        batteryCharge={powerFlow?.battery_charge_w || 0}
        batterySoc={powerFlow?.battery_soc_pct || 0}
      />

      {/* Top Circuits */}
      <Text style={styles.sectionTitle}>Top Circuits</Text>
      {powerFlow?.circuit_breakdown && 
        Object.entries(powerFlow.circuit_breakdown)
          .sort(([, a], [, b]) => b - a)
          .slice(0, 5)
          .map(([id, watts]) => (
            <CircuitGauge 
              key={id}
              circuitId={parseInt(id)}
              watts={watts}
              maxWatts={5000}
            />
          ))
      }

      {/* Bill Projection */}
      <BillProjectionCard />

      <View style={styles.spacer} />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0a0a23',
    padding: 16,
  },
  sectionTitle: {
    color: '#fff',
    fontSize: 18,
    fontWeight: '600',
    marginTop: 16,
    marginBottom: 8,
  },
  spacer: {
    height: 40,
  },
});