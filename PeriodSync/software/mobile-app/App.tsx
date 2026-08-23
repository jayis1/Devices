import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import DashboardScreen from './src/screens/DashboardScreen';
import ReliefScreen from './src/screens/ReliefScreen';
import CycleScreen from './src/screens/CycleScreen';
import ReportsScreen from './src/screens/ReportsScreen';

export default function App() {
  return (
    <SafeAreaView>
      <ScrollView>
        <View style={{ padding: 16, gap: 20 }}>
          <Text style={{ fontSize: 28, fontWeight: '700' }}>PeriodSync</Text>
          <DashboardScreen />
          <CycleScreen />
          <ReliefScreen />
          <ReportsScreen />
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}
