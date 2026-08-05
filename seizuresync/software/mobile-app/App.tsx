// SeizureSync — React Native mobile app entry
// SPDX-License-Identifier: MIT
import React from 'react';
import { StatusBar } from 'expo-status-bar';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Ionicons } from '@expo/vector-icons';

import DashboardScreen from './src/screens/DashboardScreen';
import DiaryScreen from './src/screens/DiaryScreen';
import RiskScreen from './src/screens/RiskScreen';
import AlertsScreen from './src/screens/AlertsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <SafeAreaProvider>
      <NavigationContainer>
        <StatusBar style="light" />
        <Tab.Navigator
          screenOptions={({ route }) => ({
            tabBarIcon: ({ color, size }) => {
              const icons: Record<string, keyof typeof Ionicons> = {
                Dashboard: 'pulse',
                Diary: 'journal',
                Risk: 'analytics',
                Alerts: 'notifications',
                Settings: 'settings',
              };
              return <Ionicons name={icons[route.name]} size={size} color={color} />;
            },
            tabBarActiveTintColor: '#0A1144',
            tabBarInactiveTintColor: 'gray',
            headerStyle: { backgroundColor: '#0A1144' },
            headerTintColor: '#fff',
          })}
        >
          <Tab.Screen name="Dashboard" component={DashboardScreen}
            options={{ title: 'SeizureSync' }} />
          <Tab.Screen name="Diary" component={DiaryScreen}
            options={{ title: 'Seizure Diary' }} />
          <Tab.Screen name="Risk" component={RiskScreen}
            options={{ title: 'Risk Forecast' }} />
          <Tab.Screen name="Alerts" component={AlertsScreen}
            options={{ title: 'Alerts' }} />
          <Tab.Screen name="Settings" component={SettingsScreen}
            options={{ title: 'Settings' }} />
        </Tab.Navigator>
      </NavigationContainer>
    </SafeAreaProvider>
  );
}