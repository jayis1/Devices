/**
 * JointSync Mobile App — App Entry Point
 *
 * React Native 0.73+ with TypeScript
 * Dashboard for joint health monitoring, flare prediction, and therapy management.
 *
 * License: MIT
 */

import React, { useEffect, useState } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaView, View, Text, StyleSheet } from 'react-native';

// Screens
import DashboardScreen from './src/screens/DashboardScreen';
import JointDetailScreen from './src/screens/JointDetailScreen';
import FlareForecastScreen from './src/screens/FlareForecastScreen';
import TherapyScreen from './src/screens/TherapyScreen';
import ScanScreen from './src/screens/ScanScreen';
import ReportsScreen from './src/screens/ReportsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

import { AuthProvider, useAuth } from './src/context/AuthContext';
import { LoginScreen } from './src/screens/LoginScreen';
import { ApiClient } from './src/api/client';

export type RootTabParamList = {
  Dashboard: undefined;
  Joints: undefined;
  Forecast: undefined;
  Therapy: undefined;
  Scan: undefined;
};

const Tab = createBottomTabNavigator<RootTabParamList>();

function MainApp() {
  const { user, token } = useAuth();

  if (!user || !token) {
    return <LoginScreen />;
  }

  return (
    <Tab.Navigator
      screenOptions={{
        tabBarActiveTintColor: '#0066CC',
        tabBarInactiveTintColor: '#888',
        headerStyle: { backgroundColor: '#0066CC' },
        headerTintColor: '#fff',
      }}
    >
      <Tab.Screen
        name="Dashboard"
        component={DashboardScreen}
        options={{ title: 'Joint Health' }}
      />
      <Tab.Screen
        name="Joints"
        component={JointDetailScreen}
        options={{ title: 'Joints' }}
      />
      <Tab.Screen
        name="Forecast"
        component={FlareForecastScreen}
        options={{ title: '7-Day Forecast' }}
      />
      <Tab.Screen
        name="Therapy"
        component={TherapyScreen}
        options={{ title: 'Compression' }}
      />
      <Tab.Screen
        name="Scan"
        component={ScanScreen}
        options={{ title: 'Scan' }}
      />
    </Tab.Navigator>
  );
}

export default function App() {
  return (
    <AuthProvider>
      <NavigationContainer>
        <MainApp />
      </NavigationContainer>
    </AuthProvider>
  );
}