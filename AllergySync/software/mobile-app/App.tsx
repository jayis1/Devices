/**
 * AllergySync — React Native Mobile App
 * App.tsx — Main entry point
 *
 * Features:
 * - Exposure dashboard (current pollen, risk meter, 24h forecast)
 * - Symptom journal
 * - Medication tracking
 * - Node management
 * - Allergy profile
 * - Insights
 * - History charts
 */

import React, { useState, useEffect } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { StatusBar } from 'expo-status-bar';
import { Ionicons } from '@expo/vector-icons';

import DashboardScreen from './src/screens/DashboardScreen';
import SymptomsScreen from './src/screens/SymptomsScreen';
import InsightsScreen from './src/screens/InsightsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();

const API_BASE = 'https://api.allergysync.io/api/v1';

export default function App() {
  return (
    <NavigationContainer>
      <StatusBar style="dark" />
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ focused, color, size }) => {
            let iconName: keyof typeof Ionicons.glyphMap;
            switch (route.name) {
              case 'Dashboard':
                iconName = focused ? 'leaf' : 'leaf-outline';
                break;
              case 'Symptoms':
                iconName = focused ? 'medical' : 'medical-outline';
                break;
              case 'Insights':
                iconName = focused ? 'analytics' : 'analytics-outline';
                break;
              case 'Settings':
                iconName = focused ? 'settings' : 'settings-outline';
                break;
              default:
                iconName = 'circle';
            }
            return <Ionicons name={iconName} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#2E7D32',
          tabBarInactiveTintColor: 'gray',
        })}
      >
        <Tab.Screen name="Dashboard" component={DashboardScreen} />
        <Tab.Screen name="Symptoms" component={SymptomsScreen} />
        <Tab.Screen name="Insights" component={InsightsScreen} />
        <Tab.Screen name="Settings" component={SettingsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}