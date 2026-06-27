import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';

import DashboardScreen from './src/screens/DashboardScreen';
import ScannerScreen from './src/screens/ScannerScreen';
import ActionsScreen from './src/screens/ActionsScreen';
import TimelineScreen from './src/screens/TimelineScreen';
import SettingsScreen from './src/screens/SettingsScreen';

export type RootStackParamList = {
  Dashboard: undefined;
  Scanner: undefined;
  Actions: undefined;
  Timeline: undefined;
  Settings: undefined;
};

const Tab = createBottomTabNavigator<RootStackParamList>();

export default function App() {
  return (
    <SafeAreaProvider>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={{
            tabBarActiveTintColor: '#4CAF50',
            tabBarStyle: { backgroundColor: '#1a1a1a' },
            headerStyle: { backgroundColor: '#1a1a1a' },
            headerTintColor: '#fff',
          }}
        >
          <Tab.Screen
            name="Dashboard"
            component={DashboardScreen}
            options={{ title: 'Compost' }}
          />
          <Tab.Screen
            name="Scanner"
            component={ScannerScreen}
            options={{ title: 'Scan' }}
          />
          <Tab.Screen
            name="Actions"
            component={ActionsScreen}
            options={{ title: 'Actions' }}
          />
          <Tab.Screen
            name="Timeline"
            component={TimelineScreen}
            options={{ title: 'Timeline' }}
          />
          <Tab.Screen
            name="Settings"
            component={SettingsScreen}
            options={{ title: 'Settings' }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </SafeAreaProvider>
  );
}