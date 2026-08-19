import React, { useEffect } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { CycleGuardProvider } from './src/services/CycleGuardContext';

import RideDashboardScreen from './src/screens/RideDashboardScreen';
import RoutePlannerScreen from './src/screens/RoutePlannerScreen';
import LockScreen from './src/screens/LockScreen';
import TheftAlertsScreen from './src/screens/TheftAlertsScreen';
import RideHistoryScreen from './src/screens/RideHistoryScreen';
import SafetyForecastScreen from './src/screens/SafetyForecastScreen';
import CrashReportsScreen from './src/screens/CrashReportsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <SafeAreaProvider>
      <CycleGuardProvider>
        <NavigationContainer>
          <Tab.Navigator initialRouteName="Dashboard">
            <Tab.Screen name="Dashboard" component={RideDashboardScreen}
              options={{ tabBarLabel: 'Ride' }} />
            <Tab.Screen name="RoutePlanner" component={RoutePlannerScreen}
              options={{ tabBarLabel: 'Route' }} />
            <Tab.Screen name="Lock" component={LockScreen} />
            <Tab.Screen name="TheftAlerts" component={TheftAlertsScreen}
              options={{ tabBarLabel: 'Theft' }} />
            <Tab.Screen name="History" component={RideHistoryScreen} />
            <Tab.Screen name="Forecast" component={SafetyForecastScreen}
              options={{ tabBarLabel: 'Forecast' }} />
            <Tab.Screen name="CrashReports" component={CrashReportsScreen}
              options={{ tabBarLabel: 'Crash' }} />
            <Tab.Screen name="Settings" component={SettingsScreen} />
          </Tab.Navigator>
        </NavigationContainer>
      </CycleGuardProvider>
    </SafeAreaProvider>
  );
}