import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import { WellSyncProvider } from './src/services/WellSyncContext';
import DashboardScreen from './src/screens/DashboardScreen';
import WaterScreen from './src/screens/WaterScreen';
import PumpRoomScreen from './src/screens/PumpRoomScreen';
import AdvisoryScreen from './src/screens/AdvisoryScreen';
import MaintenanceScreen from './src/screens/MaintenanceScreen';

export default function App() {
  return (
    <WellSyncProvider>
      <SafeAreaView style={{ flex: 1, backgroundColor: '#0b1220' }}>
        <ScrollView contentContainerStyle={{ padding: 16, gap: 16 }}>
          <Text style={{ color: 'white', fontSize: 28, fontWeight: '700' }}>WellSync</Text>
          <Text style={{ color: '#cbd5e1' }}>Private well safety, treatment assurance, and pump resilience.</Text>
          <DashboardScreen />
          <WaterScreen />
          <PumpRoomScreen />
          <AdvisoryScreen />
          <MaintenanceScreen />
          <View style={{ height: 24 }} />
        </ScrollView>
      </SafeAreaView>
    </WellSyncProvider>
  );
}
