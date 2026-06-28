/**
 * PestSync Mobile App — Entry Point
 * software/mobile-app/App.tsx
 */
import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Provider } from 'react-redux';
import { store } from './src/store/store';

import DashboardScreen from './src/screens/DashboardScreen';
import HeatmapScreen from './src/screens/HeatmapScreen';
import TrapsScreen from './src/screens/TrapsScreen';
import DeterrentsScreen from './src/screens/DeterrentsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

export type RootTabParamList = {
  Dashboard: undefined;
  Heatmap: undefined;
  Traps: undefined;
  Deterrents: undefined;
  Settings: undefined;
};

const Tab = createBottomTabNavigator<RootTabParamList>();

export default function App() {
  return (
    <Provider store={store}>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={{
            tabBarActiveTintColor: '#e74c3c',
            tabBarInactiveTintColor: '#95a5a6',
            headerStyle: { backgroundColor: '#2c3e50' },
            headerTintColor: '#fff',
          }}
        >
          <Tab.Screen
            name="Dashboard"
            component={DashboardScreen}
            options={{ title: 'PestSync', tabBarIcon: () => null }}
          />
          <Tab.Screen
            name="Heatmap"
            component={HeatmapScreen}
            options={{ tabBarIcon: () => null }}
          />
          <Tab.Screen
            name="Traps"
            component={TrapsScreen}
            options={{ tabBarIcon: () => null }}
          />
          <Tab.Screen
            name="Deterrents"
            component={DeterrentsScreen}
            options={{ tabBarIcon: () => null }}
          />
          <Tab.Screen
            name="Settings"
            component={SettingsScreen}
            options={{ tabBarIcon: () => null }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </Provider>
  );
}