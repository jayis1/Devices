/**
 * ErgoFlow — React Native Mobile App
 * Main entry point
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

// Screens
import DashboardScreen from './src/screens/DashboardScreen';
import PostureHistoryScreen from './src/screens/PostureHistoryScreen';
import BreakRemindersScreen from './src/screens/BreakRemindersScreen';
import DeskControlScreen from './src/screens/DeskControlScreen';
import SettingsScreen from './src/screens/SettingsScreen';

// State
import { ErgoFlowProvider } from './src/state/ErgoFlowContext';

const Tab = createBottomTabNavigator();

const App = () => {
  return (
    <ErgoFlowProvider>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={({ route }) => ({
            tabBarIcon: ({ focused, color, size }) => {
              let iconName;
              switch (route.name) {
                case 'Dashboard':
                  iconName = 'posture';
                  break;
                case 'Posture':
                  iconName = 'chart-line';
                  break;
                case 'Breaks':
                  iconName = 'timer-outline';
                  break;
                case 'Desk':
                  iconName = 'arrow-up-down';
                  break;
                case 'Settings':
                  iconName = 'cog';
                  break;
                default:
                  iconName = 'circle';
              }
              return <Icon name={iconName} size={size} color={color} />;
            },
            tabBarActiveTintColor: '#4F46E5',
            tabBarInactiveTintColor: '#9CA3AF',
            tabBarStyle: { paddingBottom: 8, height: 60 },
            headerStyle: { backgroundColor: '#4F46E5' },
            headerTintColor: '#FFFFFF',
            headerTitleStyle: { fontWeight: 'bold' },
          })}
        >
          <Tab.Screen
            name="Dashboard"
            component={DashboardScreen}
            options={{ title: 'ErgoFlow' }}
          />
          <Tab.Screen
            name="Posture"
            component={PostureHistoryScreen}
            options={{ title: 'Posture History' }}
          />
          <Tab.Screen
            name="Breaks"
            component={BreakRemindersScreen}
            options={{ title: 'Break Reminders' }}
          />
          <Tab.Screen
            name="Desk"
            component={DeskControlScreen}
            options={{ title: 'Desk Control' }}
          />
          <Tab.Screen
            name="Settings"
            component={SettingsScreen}
            options={{ title: 'Settings' }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </ErgoFlowProvider>
  );
};

export default App;