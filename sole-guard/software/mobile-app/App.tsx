/**
 * SoleGuard Mobile App — React Native
 *
 * Tabs: Home (risk + offload prompt), Heat Map, Gait, Scans, Alerts, Caregiver
 */

import React, { useState, useEffect } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Provider as PaperProvider, Text, Card, Button } from 'react-native-paper';
import { View, StyleSheet } from 'react-native';

import HomeScreen from './screens/HomeScreen';
import HeatMapScreen from './screens/HeatMapScreen';
import GaitScreen from './screens/GaitScreen';
import ScansScreen from './screens/ScansScreen';
import AlertsScreen from './screens/AlertsScreen';
import CaregiverScreen from './screens/CaregiverScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <PaperProvider>
      <SafeAreaProvider>
        <NavigationContainer>
          <Tab.Navigator
            screenOptions={{
              tabBarActiveTintColor: '#e91e63',
              headerStyle: { backgroundColor: '#1a1a2e' },
              headerTintColor: '#fff',
            }}
          >
            <Tab.Screen name="Home"    component={HomeScreen}    options={{ tabBarLabel: 'Home' }} />
            <Tab.Screen name="Heat Map" component={HeatMapScreen} options={{ tabBarLabel: 'Feet' }} />
            <Tab.Screen name="Gait"    component={GaitScreen}    options={{ tabBarLabel: 'Gait' }} />
            <Tab.Screen name="Scans"   component={ScansScreen}   options={{ tabBarLabel: 'Scans' }} />
            <Tab.Screen name="Alerts"  component={AlertsScreen}  options={{ tabBarLabel: 'Alerts' }} />
            <Tab.Screen name="Caregiver" component={CaregiverScreen} options={{ tabBarLabel: 'Share' }} />
          </Tab.Navigator>
        </NavigationContainer>
      </SafeAreaProvider>
    </PaperProvider>
  );
}