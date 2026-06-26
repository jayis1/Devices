/**
 * TrailSync Mobile App — Main Entry Point
 *
 * React Native app for trail running safety:
 * - Live trail map with offline support
 * - Biomechanics dashboard (gait, cadence, impact)
 * - Navigation and off-trail alerts
 * - Injury forecast (7-day)
 * - Altitude monitor (SpO2, HRV)
 * - Weather/storm alerts
 * - Group tracker
 * - SOS emergency button
 * - Auto-logged training journal
 *
 * SPDX-License-Identifier: MIT
 */
import React, { useState, useEffect, useCallback } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { View, Text, StyleSheet, Alert, TouchableOpacity } from 'react-native';
import { api } from './api';

// Screens
import { TrailMapScreen } from './screens/TrailMapScreen';
import { BiomechanicsScreen } from './screens/BiomechanicsScreen';
import { InjuryForecastScreen } from './screens/InjuryForecastScreen';
import { AltitudeMonitorScreen } from './screens/AltitudeMonitorScreen';
import { SOSScreen } from './screens/SOSScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  const [runnerId, setRunnerId] = useState('runner_001');
  const [connected, setConnected] = useState(false);
  const [sosActive, setSosActive] = useState(false);

  // WebSocket connection for real-time data
  useEffect(() => {
    const ws = new WebSocket(`ws://${api.getBaseUrl()}/ws/v1/live`);
    ws.onopen = () => setConnected(true);
    ws.onclose = () => setConnected(false);
    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      if (data.event === 'sos') {
        Alert.alert(
          '🚨 SOS Alert',
          `Runner ${data.data.runner_id} needs help!\n` +
          `Location: ${data.data.lat.toFixed(5)}, ${data.data.lon.toFixed(5)}\n` +
          `SpO2: ${data.data.spo2}%  HR: ${data.data.hr} bpm`,
          [{ text: 'OK' }]
        );
      } else if (data.event === 'telemetry' && data.alerts?.length > 0) {
        data.alerts.forEach(alert => {
          Alert.alert('⚠ Alert', alert.message);
        });
      }
    };
    return () => ws.close();
  }, []);

  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          tabBarStyle: { backgroundColor: '#1a1a2e' },
          tabBarActiveTintColor: '#4cc9f0',
          tabBarInactiveTintColor: '#888',
        }}
      >
        <Tab.Screen name="Map" options={{ tabBarIcon: () => <Text>🗺️</Text> }}>
          {() => <TrailMapScreen runnerId={runnerId} />}
        </Tab.Screen>
        <Tab.Screen name="Gait" options={{ tabBarIcon: () => <Text>🏃</Text> }}>
          {() => <BiomechanicsScreen runnerId={runnerId} />}
        </Tab.Screen>
        <Tab.Screen name="Injury" options={{ tabBarIcon: () => <Text>🦴</Text> }}>
          {() => <InjuryForecastScreen runnerId={runnerId} />}
        </Tab.Screen>
        <Tab.Screen name="Altitude" options={{ tabBarIcon: () => <Text>🏔️</Text> }}>
          {() => <AltitudeMonitorScreen runnerId={runnerId} />}
        </Tab.Screen>
        <Tab.Screen name="SOS" options={{ tabBarIcon: () => <Text>🆘</Text> }}>
          {() => <SOSScreen sosActive={sosActive} setSosActive={setSosActive} />}
        </Tab.Screen>
      </Tab.Navigator>
    </NavigationContainer>
  );
}