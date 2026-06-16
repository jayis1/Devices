/**
 * CradleKeep — Mobile App (React Native)
 * 
 * Main navigation: Home, Cry Feed, Feeding, Sleep, Settings
 * Connects to CradleKeep hub via BLE (local) or MQTT (cloud)
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { createStackNavigator } from '@react-navigation/stack';
import { Provider as PaperProvider } from 'react-native-paper';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

// Screens
import HomeScreen from './screens/HomeScreen';
import CryFeedScreen from './screens/CryFeedScreen';
import FeedingScreen from './screens/FeedingScreen';
import SleepScreen from './screens/SleepScreen';
import SettingsScreen from './screens/SettingsScreen';
import SetupWizardScreen from './screens/SetupWizardScreen';
import BreathingDetailScreen from './screens/BreathingDetailScreen';

// Services
import { BleService } from './services/ble';
import { MqttService } from './services/mqtt';
import { PushService } from './services/push';

const Tab = createBottomTabNavigator();
const Stack = createStackNavigator();

// ── Tab Navigator ──────────────────────────────────────────────────────────
function MainTabs() {
  return (
    <Tab.Navigator
      screenOptions={({ route }) => ({
        tabBarIcon: ({ focused, color, size }) => {
          let iconName;
          switch (route.name) {
            case 'Home':
              iconName = 'baby-face-outline';
              break;
            case 'Cry':
              iconName = 'ear-hearing';
              break;
            case 'Feeding':
              iconName = 'baby-bottle';
              break;
            case 'Sleep':
              iconName = 'sleep';
              break;
            case 'Settings':
              iconName = 'cog-outline';
              break;
          }
          return <Icon name={iconName} size={size} color={color} />;
        },
        tabBarActiveTintColor: '#4CAF50',
        tabBarInactiveTintColor: 'gray',
      })}
    >
      <Tab.Screen 
        name="Home" 
        component={HomeScreen}
        options={{ title: 'CradleKeep' }}
      />
      <Tab.Screen 
        name="Cry" 
        component={CryFeedScreen}
        options={{ title: 'Cry Monitor' }}
      />
      <Tab.Screen 
        name="Feeding" 
        component={FeedingScreen}
        options={{ title: 'Feeding' }}
      />
      <Tab.Screen 
        name="Sleep" 
        component={SleepScreen}
        options={{ title: 'Sleep' }}
      />
      <Tab.Screen 
        name="Settings" 
        component={SettingsScreen}
        options={{ title: 'Settings' }}
      />
    </Tab.Navigator>
  );
}

// ── Root Stack ──────────────────────────────────────────────────────────────
function AppContent() {
  const [isSetup, setIsSetup] = React.useState(false);
  
  return (
    <Stack.Navigator>
      {!isSetup ? (
        <Stack.Screen 
          name="Setup" 
          component={SetupWizardScreen}
          options={{ headerShown: false }}
          initialParams={{ onComplete: () => setIsSetup(true) }}
        />
      ) : (
        <>
          <Stack.Screen 
            name="Main" 
            component={MainTabs}
            options={{ headerShown: false }}
          />
          <Stack.Screen 
            name="BreathingDetail" 
            component={BreathingDetailScreen}
            options={{ title: 'Breathing Monitor' }}
          />
        </>
      )}
    </Stack.Navigator>
  );
}

// ── App Root ────────────────────────────────────────────────────────────────
export default function App() {
  return (
    <PaperProvider>
      <NavigationContainer>
        <AppContent />
      </NavigationContainer>
    </PaperProvider>
  );
}