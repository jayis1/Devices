/**
 * HiveSync — Mobile App Entry Point
 * React Native app for beehive monitoring and management
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createStackNavigator } from '@react-navigation/stack';
import { BottomTabNavigator } from './src/navigation/TabNavigator';
import { AlertProvider } from './src/context/AlertContext';
import { ApiProvider } from './src/context/ApiContext';

const Stack = createStackNavigator();

export default function App() {
  return (
    <ApiProvider>
      <AlertProvider>
        <NavigationContainer>
          <Stack.Navigator screenOptions={{ headerShown: false }}>
            <Stack.Screen name="Main" component={BottomTabNavigator} />
            <Stack.Screen name="HiveDetail" component={require('./src/screens/HiveDetailScreen').default} />
            <Stack.Screen name="SwarmAlert" component={require('./src/screens/SwarmAlertScreen').default} />
            <Stack.Screen name="VarroaMonitor" component={require('./src/screens/VarroaMonitorScreen').default} />
            <Stack.Screen name="FeederControl" component={require('./src/screens/FeederControlScreen').default} />
          </Stack.Navigator>
        </NavigationContainer>
      </AlertProvider>
    </ApiProvider>
  );
}