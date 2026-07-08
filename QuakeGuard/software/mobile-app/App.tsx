import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { PaperProvider } from 'react-native-paper';
import { Ionicons } from '@expo/vector-icons';

import LiveDashboardScreen from './src/screens/LiveDashboardScreen';
import AlertScreen from './src/screens/AlertScreen';
import StructuralHealthScreen from './src/screens/StructuralHealthScreen';
import EventHistoryScreen from './src/screens/EventHistoryScreen';
import SettingsScreen from './src/screens/SettingsScreen';

export type RootTabParamList = {
  Dashboard: undefined;
  Alerts: undefined;
  Structural: undefined;
  History: undefined;
  Settings: undefined;
};

const Tab = createBottomTabNavigator<RootTabParamList>();

export default function App() {
  return (
    <PaperProvider>
      <NavigationContainer>
        <Tab.Navigator
          screenOptions={({ route }) => ({
            tabBarIcon: ({ focused, color, size }) => {
              let iconName: keyof typeof Ionicons.glyphMap;

              switch (route.name) {
                case 'Dashboard':
                  iconName = focused ? 'shield' : 'shield-outline';
                  break;
                case 'Alerts':
                  iconName = focused ? 'warning' : 'warning-outline';
                  break;
                case 'Structural':
                  iconName = focused ? 'analytics' : 'analytics-outline';
                  break;
                case 'History':
                  iconName = focused ? 'time' : 'time-outline';
                  break;
                case 'Settings':
                  iconName = focused ? 'settings' : 'settings-outline';
                  break;
                default:
                  iconName = 'help-outline';
              }

              return <Ionicons name={iconName} size={size} color={color} />;
            },
            tabBarActiveTintColor: '#e53935',
            tabBarInactiveTintColor: 'gray',
          })}
        >
          <Tab.Screen
            name="Dashboard"
            component={LiveDashboardScreen}
            options={{ title: 'QuakeGuard' }}
          />
          <Tab.Screen
            name="Alerts"
            component={AlertScreen}
            options={{ title: 'Alerts' }}
          />
          <Tab.Screen
            name="Structural"
            component={StructuralHealthScreen}
            options={{ title: 'Structure' }}
          />
          <Tab.Screen
            name="History"
            component={EventHistoryScreen}
            options={{ title: 'History' }}
          />
          <Tab.Screen
            name="Settings"
            component={SettingsScreen}
            options={{ title: 'Settings' }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}