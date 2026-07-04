/**
 * GlucoSync Mobile App — Root App
 *
 * React Native + TypeScript
 * Tab navigation: Live Glucose, Meal Log, Insulin Log, Activity, Analytics, Settings
 *
 * License: MIT
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';

import { AuthProvider } from './src/context/AuthContext';
import LiveGlucoseScreen from './src/screens/LiveGlucoseScreen';
import MealLogScreen from './src/screens/MealLogScreen';
import InsulinLogScreen from './src/screens/InsulinLogScreen';
import AnalyticsScreen from './src/screens/AnalyticsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

export type RootTabParamList = {
  Live: undefined;
  Meals: undefined;
  Insulin: undefined;
  Analytics: undefined;
  Settings: undefined;
};

const Tab = createBottomTabNavigator<RootTabParamList>();

export default function App() {
  return (
    <SafeAreaProvider>
      <AuthProvider>
        <NavigationContainer>
          <Tab.Navigator
            screenOptions={{
              tabBarActiveTintColor: '#2563EB',
              tabBarInactiveTintColor: '#94A3B8',
              headerStyle: { backgroundColor: '#1E3A5F' },
              headerTintColor: '#FFFFFF',
            }}
          >
            <Tab.Screen
              name="Live"
              component={LiveGlucoseScreen}
              options={{ title: 'Glucose' }}
            />
            <Tab.Screen
              name="Meals"
              component={MealLogScreen}
              options={{ title: 'Meals' }}
            />
            <Tab.Screen
              name="Insulin"
              component={InsulinLogScreen}
              options={{ title: 'Insulin' }}
            />
            <Tab.Screen
              name="Analytics"
              component={AnalyticsScreen}
              options={{ title: 'Analytics' }}
            />
            <Tab.Screen
              name="Settings"
              component={SettingsScreen}
              options={{ title: 'Settings' }}
            />
          </Tab.Navigator>
        </NavigationContainer>
      </AuthProvider>
    </SafeAreaProvider>
  );
}