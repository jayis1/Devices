/**
 * FreshKeep — Mobile App (React Native)
 * 
 * Main navigation: Home, Inventory, Shopping, Recipes, Fire Safety
 * Connects to FreshKeep hub via BLE (local) or MQTT (cloud)
 */

import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { createStackNavigator } from '@react-navigation/stack';
import { Provider as PaperProvider } from 'react-native-paper';
import Icon from 'react-native-vector-icons/MaterialCommunityIcons';

// Screens
import HomeScreen from './screens/HomeScreen';
import InventoryScreen from './screens/InventoryScreen';
import ShoppingListScreen from './screens/ShoppingListScreen';
import RecipeSuggestScreen from './screens/RecipeSuggestScreen';
import FireStatusScreen from './screens/FireStatusScreen';
import SetupWizardScreen from './screens/SetupWizardScreen';
import ItemDetailScreen from './screens/ItemDetailScreen';

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
              iconName = 'home';
              break;
            case 'Inventory':
              iconName = 'fridge';
              break;
            case 'Shopping':
              iconName = 'cart';
              break;
            case 'Recipes':
              iconName = 'chef-hat';
              break;
            case 'Fire Safety':
              iconName = 'fire-alert';
              break;
          }
          return <Icon name={iconName} size={size} color={color} />;
        },
        tabBarActiveTintColor: '#2196F3',
        tabBarInactiveTintColor: 'gray',
      })}
    >
      <Tab.Screen 
        name="Home" 
        component={HomeScreen}
        options={{ title: 'FreshKeep' }}
      />
      <Tab.Screen 
        name="Inventory" 
        component={InventoryScreen}
        options={{ title: 'Fridge & Pantry' }}
      />
      <Tab.Screen 
        name="Shopping" 
        component={ShoppingListScreen}
        options={{ title: 'Shopping List' }}
      />
      <Tab.Screen 
        name="Recipes" 
        component={RecipeSuggestScreen}
        options={{ title: 'Recipes' }}
      />
      <Tab.Screen 
        name="Fire Safety" 
        component={FireStatusScreen}
        options={{ title: 'Stove Guard' }}
      />
    </Tab.Navigator>
  );
}

// ── Root Stack Navigator ──────────────────────────────────────────────────
function App() {
  const [isSetup, setIsSetup] = React.useState(false);
  const [hubConnected, setHubConnected] = React.useState(false);

  React.useEffect(() => {
    // Initialize services
    const initServices = async () => {
      try {
        await BleService.initialize();
        await MqttService.connect('freshkeep/hub/data');
        await PushService.requestPermissions();
        
        // Try to connect to hub via BLE
        const hub = await BleService.scanForHub();
        if (hub) {
          setHubConnected(true);
        }
      } catch (error) {
        console.error('Service initialization failed:', error);
      }
    };
    
    initServices();
  }, []);

  return (
    <PaperProvider>
      <NavigationContainer>
        <Stack.Navigator>
          {!isSetup ? (
            <Stack.Screen 
              name="Setup" 
              component={SetupWizardScreen}
              options={{ headerShown: false }}
              initialParams={{ onComplete: () => setIsSetup(true) }}
            />
          ) : null}
          <Stack.Screen 
            name="Main" 
            component={MainTabs} 
            options={{ headerShown: false }}
          />
          <Stack.Screen 
            name="ItemDetail" 
            component={ItemDetailScreen}
            options={{ title: 'Item Details' }}
          />
        </Stack.Navigator>
      </NavigationContainer>
    </PaperProvider>
  );
}

export default App;