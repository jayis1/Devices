/**
 * BrewSync Mobile App — Main Navigation & Screens
 *
 * React Native app for monitoring fermentation batches,
 * connecting to Brew Scanner, viewing history, and managing recipes.
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { createStackNavigator } from '@react-navigation/stack';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

// Screens
import DashboardScreen from './src/screens/DashboardScreen';
import BatchDetailScreen from './src/screens/BatchDetailScreen';
import ScannerScreen from './src/screens/ScannerScreen';
import HistoryScreen from './src/screens/HistoryScreen';
import RecipesScreen from './src/screens/RecipesScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();
const Stack = createStackNavigator();

function DashboardStack() {
  return (
    <Stack.Navigator>
      <Stack.Screen
        name="Dashboard"
        component={DashboardScreen}
        options={{ title: 'BrewSync' }}
      />
      <Stack.Screen
        name="BatchDetail"
        component={BatchDetailScreen}
        options={({ route }) => ({ title: route.params?.batchName || 'Batch' })}
      />
    </Stack.Navigator>
  );
}

export default function App() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ color, size }) => {
            let iconName;
            switch (route.name) {
              case 'Home': iconName = 'beer'; break;
              case 'Scanner': iconName = 'magnify-scan'; break;
              case 'History': iconName = 'history'; break;
              case 'Recipes': iconName = 'book-open-variant'; break;
              case 'Settings': iconName = 'cog'; break;
              default: iconName = 'circle'; break;
            }
            return <Icon name={iconName} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#D4A017',  // Beer gold
          tabBarInactiveTintColor: '#888',
          tabBarStyle: { backgroundColor: '#1a1a2e' },
          headerStyle: { backgroundColor: '#1a1a2e' },
          headerTintColor: '#fff',
        })}
      >
        <Tab.Screen name="Home" component={DashboardStack} options={{ headerShown: false }} />
        <Tab.Screen name="Scanner" component={ScannerScreen} options={{ title: 'Brew Scanner' }} />
        <Tab.Screen name="History" component={HistoryScreen} options={{ title: 'Batch History' }} />
        <Tab.Screen name="Recipes" component={RecipesScreen} options={{ title: 'Recipes' }} />
        <Tab.Screen name="Settings" component={SettingsScreen} options={{ title: 'Settings' }} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}