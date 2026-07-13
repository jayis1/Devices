/**
 * CardioSync App — Main entry point
 *
 * React Native app for cardiovascular health monitoring.
 * Shows real-time HR, ECG, BP trends, HRV, stroke risk, and reports.
 *
 * License: MIT
 */
import React, { useState, useEffect } from 'react';
import { StatusBar } from 'expo-status-bar';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Ionicons } from '@expo/vector-icons';

import LiveDashboardScreen from './src/screens/LiveDashboardScreen';
import ECGViewerScreen from './src/screens/ECGViewerScreen';
import BPTrendsScreen from './src/screens/BPTrendsScreen';
import RiskAssessmentScreen from './src/screens/RiskAssessmentScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import { ApiProvider } from './src/api/client';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <SafeAreaProvider>
      <ApiProvider>
        <NavigationContainer>
          <StatusBar style="light" />
          <Tab.Navigator
            screenOptions={{
              tabBarActiveTintColor: '#e74c3c',
              tabBarInactiveTintColor: '#95a5a6',
              headerStyle: { backgroundColor: '#2c3e50' },
              headerTintColor: '#fff',
            }}
          >
            <Tab.Screen
              name="Dashboard"
              component={LiveDashboardScreen}
              options={{
                tabBarIcon: ({ color }) => (
                  <Ionicons name="heart" size={24} color={color} />
                ),
              }}
            />
            <Tab.Screen
              name="ECG"
              component={ECGViewerScreen}
              options={{
                tabBarIcon: ({ color }) => (
                  <Ionicons name="pulse" size={24} color={color} />
                ),
              }}
            />
            <Tab.Screen
              name="Blood Pressure"
              component={BPTrendsScreen}
              options={{
                tabBarIcon: ({ color }) => (
                  <Ionicons name="speedometer" size={24} color={color} />
                ),
              }}
            />
            <Tab.Screen
              name="Risk"
              component={RiskAssessmentScreen}
              options={{
                tabBarIcon: ({ color }) => (
                  <Ionicons name="warning" size={24} color={color} />
                ),
              }}
            />
            <Tab.Screen
              name="Settings"
              component={SettingsScreen}
              options={{
                tabBarIcon: ({ color }) => (
                  <Ionicons name="settings" size={24} color={color} />
                ),
              }}
            />
          </Tab.Navigator>
        </NavigationContainer>
      </ApiProvider>
    </SafeAreaProvider>
  );
}