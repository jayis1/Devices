// SkinSync Mobile App — App.tsx
// React Native entry point
// Tab navigator: UV, Scan, Routine, Lesions, Alerts

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';

import UVDashboardScreen from './screens/UVDashboardScreen';
import ScanScreen from './screens/ScanScreen';
import RoutineScreen from './screens/RoutineScreen';
import LesionTrackerScreen from './screens/LesionTrackerScreen';
import AlertsScreen from './screens/AlertsScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ focused, color, size }) => {
            let iconName: string;
            switch (route.name) {
              case 'UV':       iconName = focused ? 'sunny' : 'sunny-outline'; break;
              case 'Scan':     iconName = focused ? 'scan' : 'scan-outline'; break;
              case 'Routine':  iconName = focused ? 'water' : 'water-outline'; break;
              case 'Lesions':  iconName = focused ? 'body' : 'body-outline'; break;
              case 'Alerts':   iconName = focused ? 'notifications' : 'notifications-outline'; break;
              default:         iconName = 'sunny';
            }
            return <Ionicons name={iconName as any} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#E91E63',
          tabBarInactiveTintColor: 'gray',
          headerShown: true,
        })}
      >
        <Tab.Screen name="UV" component={UVDashboardScreen} options={{ title: 'SkinSync' }} />
        <Tab.Screen name="Scan" component={ScanScreen} />
        <Tab.Screen name="Routine" component={RoutineScreen} />
        <Tab.Screen name="Lesions" component={LesionTrackerScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}