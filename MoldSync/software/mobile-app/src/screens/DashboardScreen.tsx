import React from 'react';
import { Text, View } from 'react-native';
import { useMoldSync } from '../services/MoldSyncContext';

export default function DashboardScreen() {
  const state = useMoldSync();
  return (
    <View>
      <Text>Mold Risk Score: {state.moldRiskScore}</Text>
      <Text>Active Alerts: {state.activeAlerts}</Text>
      <Text>Highest Risk Room: {state.highestRiskRoom}</Text>
      <Text>Driest Room: {state.driestRoom}</Text>
    </View>
  );
}
