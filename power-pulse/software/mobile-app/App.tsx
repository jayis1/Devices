/**
 * PowerPulse Mobile App — Main Entry Point (React Native)
 * 
 * AI-powered home energy intelligence dashboard.
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { StatusBar } from 'expo-status-bar';
import { ThemeProvider } from './src/theme';

// Screens
import DashboardScreen from './src/screens/DashboardScreen';
import CircuitsScreen from './src/screens/CircuitsScreen';
import AppliancesScreen from './src/screens/AppliancesScreen';
import SolarScreen from './src/screens/SolarScreen';
import AlertsScreen from './src/screens/AlertsScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <ThemeProvider>
      <NavigationContainer>
        <StatusBar style="light" />
        <Tab.Navigator
          screenOptions={{
            tabBarStyle: { backgroundColor: '#1a1a2e' },
            tabBarActiveTintColor: '#00d4aa',
            tabBarInactiveTintColor: '#666',
            headerStyle: { backgroundColor: '#16213e' },
            headerTintColor: '#fff',
          }}
        >
          <Tab.Screen 
            name="Dashboard" 
            component={DashboardScreen}
            options={{ tabBarIcon: ({ color }) => <Icon name="flash" color={color} /> }}
          />
          <Tab.Screen 
            name="Circuits" 
            component={CircuitsScreen}
            options={{ tabBarIcon: ({ color }) => <Icon name="gauge" color={color} /> }}
          />
          <Tab.Screen 
            name="Appliances" 
            component={AppliancesScreen}
            options={{ tabBarIcon: ({ color }) => <Icon name="power-plug" color={color} /> }}
          />
          <Tab.Screen 
            name="Solar" 
            component={SolarScreen}
            options={{ tabBarIcon: ({ color }) => <Icon name="white-balance-sunny" color={color} /> }}
          />
          <Tab.Screen 
            name="Alerts" 
            component={AlertsScreen}
            options={{ tabBarIcon: ({ color }) => <Icon name="alert-circle" color={color} /> }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </ThemeProvider>
  );
}