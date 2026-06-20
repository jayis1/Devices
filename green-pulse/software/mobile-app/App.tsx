// GreenPulse Mobile App — App.tsx
// React Native entry point
// Tab navigator: Home, Plants, Scan, Watering, Alerts

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';

import HomeScreen from './screens/HomeScreen';
import PlantsScreen from './screens/PlantsScreen';
import ScanScreen from './screens/ScanScreen';
import WateringScreen from './screens/WateringScreen';
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
              case 'Home':     iconName = focused ? 'home' : 'home-outline'; break;
              case 'Plants':   iconName = focused ? 'leaf' : 'leaf-outline'; break;
              case 'Scan':     iconName = focused ? 'scan' : 'scan-outline'; break;
              case 'Watering': iconName = focused ? 'water' : 'water-outline'; break;
              case 'Alerts':   iconName = focused ? 'notifications' : 'notifications-outline'; break;
              default:         iconName = 'leaf';
            }
            return <Ionicons name={iconName as any} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#2E7D32',
          tabBarInactiveTintColor: 'gray',
          headerShown: true,
        })}
      >
        <Tab.Screen name="Home" component={HomeScreen} options={{ title: 'GreenPulse' }} />
        <Tab.Screen name="Plants" component={PlantsScreen} />
        <Tab.Screen name="Scan" component={ScanScreen} />
        <Tab.Screen name="Watering" component={WateringScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}