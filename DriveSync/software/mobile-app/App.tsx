/**
 * DriveSync Mobile App — App Entry Point
 *
 * React Native 0.73+ with TypeScript
 * Dashboard for driving safety, trip history, coaching reports, and device pairing.
 *
 * License: MIT
 */

import React, { useEffect, useState } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaView, View, Text, StyleSheet } from 'react-native';

// Screens
import LiveDriveScreen from './src/screens/LiveDriveScreen';
import TripHistoryScreen from './src/screens/TripHistoryScreen';
import CoachingScreen from './src/screens/CoachingScreen';
import SettingsScreen from './src/screens/SettingsScreen';

import { AuthProvider, useAuth } from './src/context/AuthContext';
import { LoginScreen } from './src/screens/LoginScreen';

export type RootTabParamList = {
  LiveDrive: undefined;
  Trips: undefined;
  Coaching: undefined;
  Settings: undefined;
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
        tabBarActiveTintColor: '#FF4444',
        tabBarInactiveTintColor: '#888',
        headerStyle: { backgroundColor: '#1a1a2e' },
        headerTintColor: '#fff',
      }}
    >
      <Tab.Screen
        name="LiveDrive"
        component={LiveDriveScreen}
        options={{ title: 'Live Drive' }}
      />
      <Tab.Screen
        name="Trips"
        component={TripHistoryScreen}
        options={{ title: 'Trip History' }}
      />
      <Tab.Screen
        name="Coaching"
        component={CoachingScreen}
        options={{ title: 'Coaching' }}
      />
      <Tab.Screen
        name="Settings"
        component={SettingsScreen}
        options={{ title: 'Settings' }}
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