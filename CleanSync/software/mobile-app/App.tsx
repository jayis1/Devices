import React from 'react';
import { SafeAreaView, ScrollView, Text, View, StyleSheet } from 'react-native';
import DashboardScreen from './src/screens/DashboardScreen';
import RoomsScreen from './src/screens/RoomsScreen';
import ScansScreen from './src/screens/ScansScreen';
import SuppliesScreen from './src/screens/SuppliesScreen';

export default function App() {
  return (
    <SafeAreaView style={styles.root}>
      <ScrollView contentContainerStyle={styles.container}>
        <Text style={styles.title}>CleanSync</Text>
        <DashboardScreen />
        <RoomsScreen />
        <ScansScreen />
        <SuppliesScreen />
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#0B1220' },
  container: { padding: 16, gap: 16 },
  title: { color: 'white', fontSize: 30, fontWeight: '700' },
});
