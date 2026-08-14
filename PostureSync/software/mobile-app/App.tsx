/**
 * PostureSync Mobile App
 * React Native + TypeScript
 *
 * AI-powered posture correction & spinal health companion app.
 * Real-time posture score, 3D spine visualization, muscle imbalance
 * heatmap, risk forecasting, coaching, clinical reports.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaView, View, Text, StyleSheet } from 'react-native';

import DashboardScreen from './src/screens/DashboardScreen';
import SpineViewScreen from './src/screens/SpineViewScreen';
import MuscleMapScreen from './src/screens/MuscleMapScreen';
import RiskForecastScreen from './src/screens/RiskForecastScreen';
import CoachingScreen from './src/screens/CoachingScreen';

import { PostureSyncProvider } from './src/services/PostureSyncContext';
import { WebSocketService } from './src/services/WebSocketService';

const Tab = createBottomTabNavigator();

const API_BASE = 'https://api.postsync.io';
const WS_URL = 'wss://api.postsync.io/ws/realtime';

function App(): React.JSX.Element {
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const ws = new WebSocketService(WS_URL);
    ws.connect();
    ws.onConnect(() => setConnected(true));
    ws.onDisconnect(() => setConnected(false));

    return () => ws.disconnect();
  }, []);

  return (
    <PostureSyncProvider apiBase={API_BASE}>
      <SafeAreaView style={styles.container}>
        <View style={styles.header}>
          <Text style={styles.headerTitle}>PostureSync</Text>
          <View style={[styles.statusDot, { backgroundColor: connected ? '#4CAF50' : '#F44336' }]} />
        </View>
        <NavigationContainer>
          <Tab.Navigator
            screenOptions={{
              tabBarActiveTintColor: '#2196F3',
              tabBarInactiveTintColor: '#757575',
              headerShown: false,
            }}
          >
            <Tab.Screen
              name="Dashboard"
              component={DashboardScreen}
              options={{ tabBarLabel: 'Posture' }}
            />
            <Tab.Screen
              name="Spine"
              component={SpineViewScreen}
              options={{ tabBarLabel: 'Spine' }}
            />
            <Tab.Screen
              name="Muscles"
              component={MuscleMapScreen}
              options={{ tabBarLabel: 'Muscles' }}
            />
            <Tab.Screen
              name="Risk"
              component={RiskForecastScreen}
              options={{ tabBarLabel: 'Risk' }}
            />
            <Tab.Screen
              name="Coaching"
              component={CoachingScreen}
              options={{ tabBarLabel: 'Coach' }}
            />
          </Tab.Navigator>
        </NavigationContainer>
      </SafeAreaView>
    </PostureSyncProvider>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#FAFAFA',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 20,
    paddingVertical: 12,
    backgroundColor: '#FFFFFF',
    borderBottomWidth: 1,
    borderBottomColor: '#E0E0E0',
  },
  headerTitle: {
    fontSize: 20,
    fontWeight: '700',
    color: '#212121',
  },
  statusDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
});

export default App;