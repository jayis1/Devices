/**
 * MedSync - React Native Mobile App
 * App.tsx - Main navigation and entry point
 *
 * Screens:
 * - HomeScreen: Today's schedule, next dose, adherence streak
 * - ScheduleScreen: Full medication schedule, add/remove meds
 * - VitalsScreen: Heart rate, SpO2, activity charts
 * - CaregiverScreen: Caregiver dashboard (multiple patients)
 * - DoseHistory: Complete dose history with verification
 * - PillStationSetup: First-time pill station setup + med loading
 * - WearablePairing: BLE pairing for wearable tag
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

import React, { useEffect, useState } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { createStackNavigator } from '@react-navigation/stack';
import { Provider as PaperProvider } from 'react-native-paper';
import { StatusBar } from 'react-native';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

// Screens
import HomeScreen from './screens/HomeScreen';
import ScheduleScreen from './screens/ScheduleScreen';
import VitalsScreen from './screens/VitalsScreen';
import CaregiverScreen from './screens/CaregiverScreen';
import DoseHistory from './screens/DoseHistory';
import PillStationSetup from './screens/PillStationSetup';
import WearablePairing from './screens/WearablePairing';

// Services
import { MQTTService } from './services/mqtt';
import { BLEService } from './services/ble';
import { PushService } from './services/push';

const Tab = createBottomTabNavigator();
const Stack = createStackNavigator();

// Theme colors — warm, healthcare-inspired palette
const COLORS = {
  primary: '#4A90D9',        // Trust blue
  primaryDark: '#2E6BB0',
  secondary: '#7BC67E',       // Health green
  accent: '#FF6B35',          // Alert orange
  success: '#2D6A4F',         // Confirmed green
  danger: '#D00000',           // Overdue red
  warning: '#FF9F1C',          // Warning yellow
  info: '#3A86FF',             // Info blue
  background: '#F8F9FA',
  surface: '#FFFFFF',
  text: '#212529',
  textSecondary: '#6C757D',
  card: '#FFFFFF',
  dosePending: '#4A90D9',     // Blue = pending
  doseTaken: '#2D6A4F',       // Green = taken
  doseOverdue: '#D00000',      // Red = overdue
  doseMissed: '#6C757D',       // Gray = missed
};

function TabNavigator() {
  return (
    <Tab.Navigator
      screenOptions={({ route }) => ({
        tabBarIcon: ({ focused, color, size }) => {
          let iconName;
          switch (route.name) {
            case 'Home':
              iconName = 'pill';
              break;
            case 'Schedule':
              iconName = 'calendar-clock';
              break;
            case 'Vitals':
              iconName = 'heart-pulse';
              break;
            case 'History':
              iconName = 'history';
              break;
            case 'Caregiver':
              iconName = 'account-group';
              break;
          }
          return <Icon name={iconName} size={size} color={color} />;
        },
        tabBarActiveTintColor: COLORS.primary,
        tabBarInactiveTintColor: COLORS.textSecondary,
        tabBarStyle: { backgroundColor: COLORS.surface },
        headerStyle: { backgroundColor: COLORS.primary },
        headerTintColor: '#FFFFFF',
      })}
    >
      <Tab.Screen
        name="Home"
        component={HomeScreen}
        options={{ title: 'MedSync' }}
      />
      <Tab.Screen
        name="Schedule"
        component={ScheduleScreen}
        options={{ title: 'Schedule' }}
      />
      <Tab.Screen
        name="Vitals"
        component={VitalsScreen}
        options={{ title: 'Vitals' }}
      />
      <Tab.Screen
        name="History"
        component={DoseHistory}
        options={{ title: 'History' }}
      />
      <Tab.Screen
        name="Caregiver"
        component={CaregiverScreen}
        options={{ title: 'Caregiver' }}
      />
    </Tab.Navigator>
  );
}

export default function App() {
  const [isSetup, setIsSetup] = useState(false);
  const [mqttConnected, setMqttConnected] = useState(false);

  useEffect(() => {
    const init = async () => {
      // Connect to MQTT broker
      try {
        await MQTTService.connect();
        setMqttConnected(true);
      } catch (error) {
        console.error('MQTT connection failed:', error);
      }

      // Register for push notifications
      await PushService.register();

      // Start BLE scan for hub discovery
      BLEService.startScan();
    };

    init();

    return () => {
      MQTTService.disconnect();
      BLEService.stopScan();
    };
  }, []);

  return (
    <PaperProvider>
      <StatusBar barStyle="light-content" backgroundColor={COLORS.primary} />
      <NavigationContainer>
        <Stack.Navigator>
          {!isSetup ? (
            <Stack.Screen
              name="Setup"
              component={PillStationSetup}
              options={{ headerShown: false }}
            />
          ) : null}
          <Stack.Screen
            name="Main"
            component={TabNavigator}
            options={{ headerShown: false }}
          />
          <Stack.Screen
            name="WearablePairing"
            component={WearablePairing}
            options={{ title: 'Pair Wearable' }}
          />
        </Stack.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}