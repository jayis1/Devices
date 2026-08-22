import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import { UroSyncProvider } from './src/services/UroSyncContext';
import DashboardScreen from './src/screens/DashboardScreen';
import HydrationScreen from './src/screens/HydrationScreen';
import NightSafetyScreen from './src/screens/NightSafetyScreen';
import TrendsScreen from './src/screens/TrendsScreen';
import SuppliesScreen from './src/screens/SuppliesScreen';
import CareCircleScreen from './src/screens/CareCircleScreen';

export default function App() {
  return (
    <UroSyncProvider>
      <SafeAreaView style={{ flex: 1, backgroundColor: '#081421' }}>
        <ScrollView contentContainerStyle={{ padding: 16, gap: 16 }}>
          <Text style={{ color: '#f8fafc', fontSize: 28, fontWeight: '700' }}>UroSync</Text>
          <Text style={{ color: '#bfdbfe' }}>Bathroom health, hydration, nocturia, and nighttime safety intelligence.</Text>
          <DashboardScreen />
          <HydrationScreen />
          <NightSafetyScreen />
          <TrendsScreen />
          <SuppliesScreen />
          <CareCircleScreen />
          <View style={{ height: 24 }} />
        </ScrollView>
      </SafeAreaView>
    </UroSyncProvider>
  );
}
