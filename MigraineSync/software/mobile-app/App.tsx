/**
 * MigraineSync — Mobile App Entry Point
 * ======================================
 * React Native (Expo) app for migraine trigger tracking and prevention.
 *
 * License: MIT
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { StatusBar } from 'expo-status-bar';
import { Ionicons } from '@expo/vector-icons';

import DashboardScreen from './src/screens/DashboardScreen';
import TriggerHeatmap from './src/screens/TriggerHeatmap';
import HydrationScreen from './src/screens/HydrationScreen';
import ActionPlanScreen from './src/screens/ActionPlanScreen';
import EventLogScreen from './src/screens/EventLogScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <StatusBar style="light" />
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: '#6C5CE7',
          tabBarInactiveTintColor: '#636E72',
          headerShown: false,
          tabBarStyle: { backgroundColor: '#2D3436' },
        }}
      >
        <Tab.Screen
          name="Dashboard"
          component={DashboardScreen}
          options={{
            tabBarIcon: ({ color, size }) => (
              <Ionicons name="pulse" color={color} size={size} />
            ),
          }}
        />
        <Tab.Screen
          name="Triggers"
          component={TriggerHeatmap}
          options={{
            tabBarIcon: ({ color, size }) => (
              <Ionicons name="analytics" color={color} size={size} />
            ),
          }}
        />
        <Tab.Screen
          name="Hydration"
          component={HydrationScreen}
          options={{
            tabBarIcon: ({ color, size }) => (
              <Ionicons name="water" color={color} size={size} />
            ),
          }}
        />
        <Tab.Screen
          name="Action Plan"
          component={ActionPlanScreen}
          options={{
            tabBarIcon: ({ color, size }) => (
              <Ionicons name="medkit" color={color} size={size} />
            ),
          }}
        />
        <Tab.Screen
          name="Events"
          component={EventLogScreen}
          options={{
            tabBarIcon: ({ color, size }) => (
              <Ionicons name="list" color={color} size={size} />
            ),
          }}
        />
      </Tab.Navigator>
    </NavigationContainer>
  );
}