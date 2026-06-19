// PawSync Mobile App — App.tsx
// React Native entry point
//
// Tab navigator: Home, Activity, Feeding, Anxiety, Vitals, Alerts, Vet

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';

import HomeScreen from './screens/HomeScreen';
import ActivityScreen from './screens/ActivityScreen';
import FeedingScreen from './screens/FeedingScreen';
import AnxietyScreen from './screens/AnxietyScreen';
import VitalsScreen from './screens/VitalsScreen';
import AlertsScreen from './screens/AlertsScreen';
import VetScreen from './screens/VetScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ focused, color, size }) => {
            let iconName: string;
            switch (route.name) {
              case 'Home':      iconName = focused ? 'paw' : 'paw-outline'; break;
              case 'Activity':  iconName = focused ? 'walk' : 'walk-outline'; break;
              case 'Feeding':   iconName = focused ? 'restaurant' : 'restaurant-outline'; break;
              case 'Anxiety':   iconName = focused ? 'heart' : 'heart-outline'; break;
              case 'Vitals':    iconName = focused ? 'pulse' : 'pulse-outline'; break;
              case 'Alerts':    iconName = focused ? 'notifications' : 'notifications-outline'; break;
              case 'Vet':       iconName = focused ? 'medkit' : 'medkit-outline'; break;
              default:          iconName = 'paw';
            }
            return <Ionicons name={iconName as any} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#2196F3',
          tabBarInactiveTintColor: 'gray',
          headerShown: true,
        })}
      >
        <Tab.Screen name="Home" component={HomeScreen} options={{ title: 'PawSync' }} />
        <Tab.Screen name="Activity" component={ActivityScreen} />
        <Tab.Screen name="Feeding" component={FeedingScreen} />
        <Tab.Screen name="Anxiety" component={AnxietyScreen} />
        <Tab.Screen name="Vitals" component={VitalsScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
        <Tab.Screen name="Vet" component={VetScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}