// CalmGrid Mobile App — App.tsx
// React Native entry point
//
// Tab navigator: Home, Stress, Interventions, Vitals, Burnout, Therapist, Alerts

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';

import HomeScreen from './screens/HomeScreen';
import StressScreen from './screens/StressScreen';
import InterventionsScreen from './screens/InterventionsScreen';
import VitalsScreen from './screens/VitalsScreen';
import BurnoutScreen from './screens/BurnoutScreen';
import TherapistScreen from './screens/TherapistScreen';
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
              case 'Home':          iconName = focused ? 'home' : 'home-outline'; break;
              case 'Stress':        iconName = focused ? 'flash' : 'flash-outline'; break;
              case 'Interventions': iconName = focused ? 'leaf' : 'leaf-outline'; break;
              case 'Vitals':        iconName = focused ? 'pulse' : 'pulse-outline'; break;
              case 'Burnout':       iconName = focused ? 'warning' : 'warning-outline'; break;
              case 'Therapist':     iconName = focused ? 'medkit' : 'medkit-outline'; break;
              case 'Alerts':        iconName = focused ? 'notifications' : 'notifications-outline'; break;
              default:              iconName = 'home';
            }
            return <Ionicons name={iconName as any} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#6C63FF',
          tabBarInactiveTintColor: 'gray',
          headerShown: true,
        })}
      >
        <Tab.Screen name="Home" component={HomeScreen} options={{ title: 'CalmGrid' }} />
        <Tab.Screen name="Stress" component={StressScreen} />
        <Tab.Screen name="Interventions" component={InterventionsScreen} />
        <Tab.Screen name="Vitals" component={VitalsScreen} />
        <Tab.Screen name="Burnout" component={BurnoutScreen} />
        <Tab.Screen name="Therapist" component={TherapistScreen} />
        <Tab.Screen name="Alerts" component={AlertsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}