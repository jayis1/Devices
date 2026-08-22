import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import { WasteSortProvider } from './src/services/WasteSortContext';
import DashboardScreen from './src/screens/DashboardScreen';
import SortGuideScreen from './src/screens/SortGuideScreen';
import BinsScreen from './src/screens/BinsScreen';
import PickupScreen from './src/screens/PickupScreen';
import ImpactScreen from './src/screens/ImpactScreen';
import SettingsScreen from './src/screens/SettingsScreen';

export default function App() {
  return (
    <WasteSortProvider>
      <SafeAreaView style={{ flex: 1, backgroundColor: '#0f172a' }}>
        <ScrollView contentContainerStyle={{ padding: 16, gap: 16 }}>
          <Text style={{ color: 'white', fontSize: 28, fontWeight: '700' }}>WasteSort</Text>
          <Text style={{ color: '#cbd5e1' }}>Household waste diversion, contamination prevention, and pickup orchestration.</Text>
          <DashboardScreen />
          <SortGuideScreen />
          <BinsScreen />
          <PickupScreen />
          <ImpactScreen />
          <SettingsScreen />
          <View style={{ height: 24 }} />
        </ScrollView>
      </SafeAreaView>
    </WasteSortProvider>
  );
}
