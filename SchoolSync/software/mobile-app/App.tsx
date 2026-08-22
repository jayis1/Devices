import React from 'react';
import { SafeAreaView, ScrollView, Text } from 'react-native';
import { SchoolSyncProvider } from './src/services/SchoolSyncContext';
import DashboardScreen from './src/screens/DashboardScreen';

export default function App() {
  return (
    <SchoolSyncProvider>
      <SafeAreaView>
        <ScrollView>
          <Text style={{ fontSize: 28, fontWeight: '700', margin: 16 }}>SchoolSync</Text>
          <DashboardScreen />
        </ScrollView>
      </SafeAreaView>
    </SchoolSyncProvider>
  );
}
