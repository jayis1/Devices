import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaView, StatusBar } from 'react-native';

import DashboardScreen from './src/screens/DashboardScreen';
import BrushSessionScreen from './src/screens/BrushSessionScreen';
import ScanScreen from './src/screens/ScanScreen';
import RiskScreen from './src/screens/RiskScreen';
import HistoryScreen from './src/screens/HistoryScreen';
import SettingsScreen from './src/screens/SettingsScreen';

export type RootStackParamList = {
  Dashboard: undefined;
  Brush: undefined;
  Scan: undefined;
  Risk: undefined;
  History: undefined;
  Settings: undefined;
};

const Tab = createBottomTabNavigator<RootStackParamList>();

export default function App() {
  return (
    <SafeAreaView style={{ flex: 1, backgroundColor: '#0a1929' }}>
      <StatusBar barStyle="light-content" />
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={{
            tabBarStyle: { backgroundColor: '#0a1929', borderTopColor: '#1a3a5c' },
            tabBarActiveTintColor: '#4fc3f7',
            tabBarInactiveTintColor: '#5a7a9a',
            headerStyle: { backgroundColor: '#0a1929' },
            headerTintColor: '#e3f2fd',
          }}
        >
          <Tab.Screen name="Dashboard" component={DashboardScreen} options={{ title: 'Mouth' }} />
          <Tab.Screen name="Brush" component={BrushSessionScreen} options={{ title: 'Brush' }} />
          <Tab.Screen name="Scan" component={ScanScreen} options={{ title: 'Scan' }} />
          <Tab.Screen name="Risk" component={RiskScreen} options={{ title: 'Risk' }} />
          <Tab.Screen name="History" component={HistoryScreen} options={{ title: 'History' }} />
          <Tab.Screen name="Settings" component={SettingsScreen} options={{ title: 'Settings' }} />
        </Tab.Navigator>
      </NavigationContainer>
    </SafeAreaView>
  );
}