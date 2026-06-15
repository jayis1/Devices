/**
 * FlowGuard - React Native Mobile App
 * App.tsx - Main navigation and entry point
 *
 * Screens:
 * - HomeScreen: Live flow gauge, valve status, recent alerts
 * - UsageScreen: Per-appliance water usage breakdown
 * - AlertHistory: Push notification history
 * - ValveControl: Remote open/close with 2FA
 * - SensorMap: Home floor plan with sensor locations
 * - SetupWizard: First-time Zigbee pairing + sensor placement
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
import UsageScreen from './screens/UsageScreen';
import AlertHistory from './screens/AlertHistory';
import ValveControl from './screens/ValveControl';
import SensorMap from './screens/SensorMap';
import SetupWizard from './screens/SetupWizard';

// Services
import { MQTTService } from './services/mqtt';
import { BLEService } from './services/ble';
import { PushService } from './services/push';

const Tab = createBottomTabNavigator();
const Stack = createStackNavigator();

// Theme colors
const COLORS = {
  primary: '#0077B6',      // Water blue
  primaryDark: '#005F8A',
  secondary: '#00B4D8',
  accent: '#FF6B35',       // Alert orange
  success: '#2D6A4F',      // Valve open green
  danger: '#D00000',       // Valve closed / emergency red
  warning: '#FF9F1C',      // Warning yellow
  background: '#F8F9FA',
  surface: '#FFFFFF',
  text: '#212529',
  textSecondary: '#6C757D',
};

function TabNavigator() {
  return (
    <Tab.Navigator
      screenOptions={({ route }) => ({
        tabBarIcon: ({ focused, color, size }) => {
          let iconName;
          switch (route.name) {
            case 'Home':
              iconName = 'home-flood';
              break;
            case 'Usage':
              iconName = 'water';
              break;
            case 'Alerts':
              iconName = 'bell-alert';
              break;
            case 'Valve':
              iconName = 'valve';
              break;
            case 'Sensors':
              iconName = 'map-marker-radius';
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
        options={{ title: 'FlowGuard' }}
      />
      <Tab.Screen
        name="Usage"
        component={UsageScreen}
        options={{ title: 'Water Usage' }}
      />
      <Tab.Screen
        name="Alerts"
        component={AlertHistory}
        options={{ title: 'Alerts' }}
      />
      <Tab.Screen
        name="Valve"
        component={ValveControl}
        options={{ title: 'Valve Control' }}
      />
      <Tab.Screen
        name="Sensors"
        component={SensorMap}
        options={{ title: 'Sensor Map' }}
      />
    </Tab.Navigator>
  );
}

export default function App() {
  const [isSetup, setIsSetup] = useState(false);
  const [mqttConnected, setMqttConnected] = useState(false);

  useEffect(() => {
    // Initialize services
    const init = async () => {
      // Check if setup has been completed
      // const setupComplete = await AsyncStorage.getItem('setup_complete');
      // setIsSetup(setupComplete === 'true');

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
              component={SetupWizard}
              options={{ headerShown: false }}
            />
          ) : null}
          <Stack.Screen
            name="Main"
            component={TabNavigator}
            options={{ headerShown: false }}
          />
        </Stack.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}