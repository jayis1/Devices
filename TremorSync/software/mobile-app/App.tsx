import React, { useEffect } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { TremorSyncProvider } from './src/services/TremorSyncContext';

import DashboardScreen from './src/screens/DashboardScreen';
import TremorViewScreen from './src/screens/TremorViewScreen';
import GaitViewScreen from './src/screens/GaitViewScreen';
import VoiceViewScreen from './src/screens/VoiceViewScreen';
import MedicationScreen from './src/screens/MedicationScreen';
import FallRiskScreen from './src/screens/FallRiskScreen';
import ProgressionScreen from './src/screens/ProgressionScreen';
import CaregiverScreen from './src/screens/CaregiverScreen';
import ReportsScreen from './src/screens/ReportsScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <SafeAreaProvider>
      <TremorSyncProvider>
        <NavigationContainer>
          <Tab.Navigator initialRouteName="Dashboard">
            <Tab.Screen name="Dashboard" component={DashboardScreen}
              options={{ tabBarLabel: 'Home' }} />
            <Tab.Screen name="Tremor" component={TremorViewScreen} />
            <Tab.Screen name="Gait" component={GaitViewScreen} />
            <Tab.Screen name="Voice" component={VoiceViewScreen} />
            <Tab.Screen name="Medication" component={MedicationScreen} />
            <Tab.Screen name="FallRisk" component={FallRiskScreen}
              options={{ tabBarLabel: 'Fall Risk' }} />
            <Tab.Screen name="Progression" component={ProgressionScreen} />
            <Tab.Screen name="Caregiver" component={CaregiverScreen} />
            <Tab.Screen name="Reports" component={ReportsScreen} />
            <Tab.Screen name="Settings" component={SettingsScreen} />
          </Tab.Navigator>
        </NavigationContainer>
      </TremorSyncProvider>
    </SafeAreaProvider>
  );
}