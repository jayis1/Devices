/**
 * AsthmaSync — React Native Mobile App
 * ======================================
 * Root component with tab navigation.
 *
 * Tabs:
 *   1. Dashboard — real-time risk gauge, air quality, last events
 *   2. Triggers — personal trigger heatmap
 *   3. Medication — adherence calendar, dose log
 *   4. Action Plan — GINA-aligned zone-based plan
 *
 * License: MIT
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';

import DashboardScreen from './src/screens/DashboardScreen';
import TriggerHeatmap from './src/screens/TriggerHeatmap';
import MedicationScreen from './src/screens/MedicationScreen';
import ActionPlanScreen from './src/screens/ActionPlanScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <SafeAreaProvider>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={{
            tabBarActiveTintColor: '#0066CC',
            tabBarInactiveTintColor: '#999',
            headerStyle: { backgroundColor: '#f0f4f8' },
            headerTitleStyle: { fontWeight: 'bold' },
          }}
        >
          <Tab.Screen
            name="Dashboard"
            component={DashboardScreen}
            options={{ title: 'AsthmaSync' }}
          />
          <Tab.Screen
            name="Triggers"
            component={TriggerHeatmap}
            options={{ title: 'My Triggers' }}
          />
          <Tab.Screen
            name="Medication"
            component={MedicationScreen}
            options={{ title: 'Medication' }}
          />
          <Tab.Screen
            name="Action Plan"
            component={ActionPlanScreen}
            options={{ title: 'Action Plan' }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </SafeAreaProvider>
  );
}