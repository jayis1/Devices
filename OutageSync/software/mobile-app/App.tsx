import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import { OutageSyncProvider } from './src/services/OutageSyncContext';
import DashboardScreen from './src/screens/DashboardScreen';
import LoadsScreen from './src/screens/LoadsScreen';
import ColdChainScreen from './src/screens/ColdChainScreen';
import GeneratorScreen from './src/screens/GeneratorScreen';
import PrepPlanScreen from './src/screens/PrepPlanScreen';

export default function App() {
  return (
    <OutageSyncProvider>
      <SafeAreaView style={{ flex: 1, backgroundColor: '#08111f' }}>
        <ScrollView contentContainerStyle={{ padding: 16, gap: 16 }}>
          <Text style={{ color: 'white', fontSize: 30, fontWeight: '700' }}>OutageSync</Text>
          <Text style={{ color: '#cbd5e1' }}>Home outage resilience, critical-load control, cold-chain safety, and generator guidance.</Text>
          <DashboardScreen />
          <LoadsScreen />
          <ColdChainScreen />
          <GeneratorScreen />
          <PrepPlanScreen />
          <View style={{ height: 24 }} />
        </ScrollView>
      </SafeAreaView>
    </OutageSyncProvider>
  );
}
