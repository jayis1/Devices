import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';
import DashboardScreen from './src/screens/DashboardScreen';

export default function App() {
  return (
    <SafeAreaView>
      <ScrollView>
        <View style={{ padding: 16 }}>
          <Text style={{ fontSize: 28, fontWeight: '700', marginBottom: 12 }}>MoldSync</Text>
          <DashboardScreen />
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}
