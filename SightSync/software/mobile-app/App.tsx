/**
 * SightSync Mobile App — Root Component
 *
 * React Native (iOS + Android)
 * License: MIT
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { PaperProvider } from 'react-native-paper';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

import LiveDashboardScreen from './src/screens/LiveDashboardScreen';
import BreakReminderScreen from './src/screens/BreakReminderScreen';
import MyopiaTrackingScreen from './src/screens/MyopiaTrackingScreen';
import LampControlScreen from './src/screens/LampControlScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import { AuthProvider } from './src/context/AuthContext';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <AuthProvider>
      <PaperProvider>
        <NavigationContainer>
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
              component={LiveDashboardScreen}
              options={{
                tabBarIcon: ({ color, size }) => (
                  <Icon name="eye" color={color} size={size} />
                ),
                title: 'Eye Health',
              }}
            />
            <Tab.Screen
              name="Breaks"
              component={BreakReminderScreen}
              options={{
                tabBarIcon: ({ color, size }) => (
                  <Icon name="timer-sand" color={color} size={size} />
                ),
                title: '20-20-20',
              }}
            />
            <Tab.Screen
              name="Myopia"
              component={MyopiaTrackingScreen}
              options={{
                tabBarIcon: ({ color, size }) => (
                  <Icon name="chart-line" color={color} size={size} />
                ),
                title: 'Myopia Forecast',
              }}
            />
            <Tab.Screen
              name="Lamp"
              component={LampControlScreen}
              options={{
                tabBarIcon: ({ color, size }) => (
                  <Icon name="lamp" color={color} size={size} />
                ),
                title: 'Smart Lamp',
              }}
            />
            <Tab.Screen
              name="Settings"
              component={SettingsScreen}
              options={{
                tabBarIcon: ({ color, size }) => (
                  <Icon name="cog" color={color} size={size} />
                ),
              }}
            />
          </Tab.Navigator>
        </NavigationContainer>
      </PaperProvider>
    </AuthProvider>
  );
}