import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import { DashboardScreen } from './src/screens/DashboardScreen';
import { FindScreen } from './src/screens/FindScreen';
import { FocusScreen } from './src/screens/FocusScreen';
import { RoutineScreen } from './src/screens/RoutineScreen';

export default function App(): JSX.Element {
  return (
    <SafeAreaView>
      <ScrollView contentInsetAdjustmentBehavior="automatic">
        <View style={{ padding: 16, gap: 18 }}>
          <Text style={{ fontSize: 28, fontWeight: '700' }}>RoutineSync</Text>
          <DashboardScreen />
          <RoutineScreen />
          <FindScreen />
          <FocusScreen />
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}
