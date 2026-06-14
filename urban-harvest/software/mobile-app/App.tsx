/**
 * UrbanHarvest - Mobile App (React Native)
 * Main application component
 *
 * Screens:
 * 1. GardenDashboard  — Bird's-eye view of all plants
 * 2. PlantDetail       — Individual plant card with readings + history
 * 3. GrowPodControl    — Light spectrum sliders, climate controls
 * 4. WeatherScreen      — Current outdoor conditions
 * 5. HarvestCalendar    — What's ready now, what's coming
 * 6. PlantingAdvisor    — AI planting suggestions
 * 7. AlertsScreen       — Active alerts + push notifications
 * 8. SettingsScreen     — Hub pairing, thresholds, plant profiles
 */

import React, { useState, useEffect } from 'react';
import {
  SafeAreaView,
  ScrollView,
  StatusBar,
  StyleSheet,
  Text,
  View,
  TouchableOpacity,
  FlatList,
  Alert,
} from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';

const API_BASE = 'http://urbanharvest.local:8000/api';

// ========== TYPES ==========

interface PlantSummary {
  id: number;
  name: string;
  plant_type: string;
  health_index: number;
  health_category: number;
  moisture_pct: number;
  last_watered: string;
  days_to_harvest?: number;
}

interface WeatherData {
  temperature_c: number;
  humidity_pct: number;
  wind_speed_kmh: number;
  rain_mm: number;
  uv_index: number;
  rain_predicted: boolean;
}

interface GardenSummary {
  total_plants: number;
  average_health: number;
  active_alerts: number;
  harvest_ready_soon: number;
  garden_status: string;
}

// ========== COLOR HELPERS ==========

const healthColor = (health: number): string => {
  if (health >= 80) return '#4CAF50';  // Green - thriving
  if (health >= 60) return '#8BC34A';  // Light green - good
  if (health >= 35) return '#FF9800';  // Orange - stressed
  return '#F44336';                     // Red - critical
};

const healthLabel = (category: number): string => {
  const labels = ['Thriving', 'Good', 'Stressed', 'Critical', 'Dead'];
  return labels[category] || 'Unknown';
};

// ========== GARDEN DASHBOARD ==========

const GardenDashboard = () => {
  const [garden, setGarden] = useState<GardenSummary | null>(null);
  const [plants, setPlants] = useState<PlantSummary[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const [gardenRes, plantsRes] = await Promise.all([
          fetch(`${API_BASE}/garden/summary`),
          fetch(`${API_BASE}/plants`),
        ]);
        const gardenData = await gardenRes.json();
        const plantsData = await plantsRes.json();
        setGarden(gardenData);
        setPlants(plantsData);
      } catch (e) {
        console.error('Failed to fetch garden data:', e);
      } finally {
        setLoading(false);
      }
    };
    fetchData();
    const interval = setInterval(fetchData, 30000); // Refresh every 30s
    return () => clearInterval(interval);
  }, []);

  if (loading) {
    return (
      <View style={styles.centered}>
        <Text style={styles.loadingText}>🌱 Loading your garden...</Text>
      </View>
    );
  }

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="dark-content" />

      {/* Garden Overview Card */}
      <View style={[styles.card, { borderLeftWidth: 4, borderLeftColor: healthColor(garden?.average_health || 0) }]}>
        <Text style={styles.cardTitle}>🌿 Garden Overview</Text>
        <View style={styles.row}>
          <View style={styles.statBox}>
            <Text style={styles.statValue}>{garden?.total_plants || 0}</Text>
            <Text style={styles.statLabel}>Plants</Text>
          </View>
          <View style={styles.statBox}>
            <Text style={[styles.statValue, { color: healthColor(garden?.average_health || 0) }]}>
              {garden?.average_health || 0}%
            </Text>
            <Text style={styles.statLabel}>Health</Text>
          </View>
          <View style={styles.statBox}>
            <Text style={[styles.statValue, { color: garden?.active_alerts ? '#F44336' : '#4CAF50' }]}>
              {garden?.active_alerts || 0}
            </Text>
            <Text style={styles.statLabel}>Alerts</Text>
          </View>
          <View style={styles.statBox}>
            <Text style={[styles.statValue, { color: '#FF9800' }]}>
              {garden?.harvest_ready_soon || 0}
            </Text>
            <Text style={styles.statLabel}>Harvest</Text>
          </View>
        </View>
      </View>

      {/* Plant List */}
      <FlatList
        data={plants}
        keyExtractor={(item) => item.id.toString()}
        renderItem={({ item }) => (
          <TouchableOpacity style={[styles.plantCard, { borderLeftColor: healthColor(item.health_index) }]}>
            <View style={styles.plantHeader}>
              <Text style={styles.plantEmoji}>
                {item.plant_type === 'tomato' ? '🍅' :
                 item.plant_type === 'basil' ? '🌿' :
                 item.plant_type === 'lettuce' ? '🥬' :
                 item.plant_type === 'pepper' ? '🌶️' :
                 item.plant_type === 'mint' ? '🌱' :
                 item.plant_type === 'strawberry' ? '🍓' :
                 item.plant_type === 'cucumber' ? '🥒' : '🌱'}
              </Text>
              <View style={styles.plantInfo}>
                <Text style={styles.plantName}>{item.name}</Text>
                <Text style={styles.plantType}>{item.plant_type}</Text>
              </View>
              <View style={[styles.healthBadge, { backgroundColor: healthColor(item.health_index) }]}>
                <Text style={styles.healthBadgeText}>{item.health_index}%</Text>
              </View>
            </View>
            <View style={styles.plantMetrics}>
              <Text style={styles.metricText}>💧 {item.moisture_pct?.toFixed(0) || '--'}%</Text>
              <Text style={styles.metricText}>📅 Harvest: {item.days_to_harvest ? `${item.days_to_harvest}d` : '?'}</Text>
            </View>
          </TouchableOpacity>
        )}
        contentContainerStyle={styles.plantList}
      />
    </SafeAreaView>
  );
};

// ========== WEATHER SCREEN ==========

const WeatherScreen = () => {
  const [weather, setWeather] = useState<WeatherData | null>(null);

  useEffect(() => {
    const fetchWeather = async () => {
      try {
        const res = await fetch(`${API_BASE}/weather`);
        setWeather(await res.json());
      } catch (e) {
        console.error('Weather fetch failed:', e);
      }
    };
    fetchWeather();
    const interval = setInterval(fetchWeather, 60000);
    return () => clearInterval(interval);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>🌤️ Weather Station</Text>
        {weather ? (
          <View>
            <Text style={styles.weatherTemp}>{weather.temperature_c?.toFixed(1)}°C</Text>
            <Text style={styles.weatherDetail}>Humidity: {weather.humidity_pct?.toFixed(0)}%</Text>
            <Text style={styles.weatherDetail}>Wind: {weather.wind_speed_kmh?.toFixed(1)} km/h</Text>
            <Text style={styles.weatherDetail}>Rain: {weather.rain_mm?.toFixed(1)} mm</Text>
            <Text style={styles.weatherDetail}>UV Index: {weather.uv_index?.toFixed(1)}</Text>
            {weather.rain_predicted && (
              <View style={styles.rainAlert}>
                <Text style={styles.rainAlertText}>🌧️ Rain expected — outdoor watering skipped</Text>
              </View>
            )}
          </View>
        ) : (
          <Text style={styles.loadingText}>Loading weather data...</Text>
        )}
      </View>
    </SafeAreaView>
  );
};

// ========== HARVEST CALENDAR ==========

const HarvestCalendar = () => {
  const [predictions, setPredictions] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/harvest/predictions`)
      .then(res => res.json())
      .then(setPredictions)
      .catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.screenTitle}>🍅 Harvest Calendar</Text>
      <FlatList
        data={predictions}
        keyExtractor={(item, idx) => idx.toString()}
        renderItem={({ item }) => (
          <View style={styles.card}>
            <Text style={styles.plantName}>Plant #{item.plant_id}</Text>
            <Text style={styles.metricText}>📅 {item.days_until} days until harvest</Text>
            <Text style={styles.metricText}>⚖️ ~{item.estimated_yield_g?.toFixed(0)}g expected</Text>
            <Text style={styles.metricText}>🎯 {item.confidence_pct}% confidence</Text>
          </View>
        )}
      />
    </SafeAreaView>
  );
};

// ========== PLANTING ADVISOR ==========

const PlantingAdvisor = () => {
  const [advice, setAdvice] = useState<any>(null);

  useEffect(() => {
    fetch(`${API_BASE}/planting/advice`)
      .then(res => res.json())
      .then(setAdvice)
      .catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.screenTitle}>🌱 Planting Advisor</Text>
      <View style={styles.card}>
        {advice ? (
          <View>
            <Text style={styles.cardTitle}>Plant This Month</Text>
            {advice.plant_now?.map((plant: string, i: number) => (
              <Text key={i} style={styles.adviceItem}>
                🌿 {plant.replace('_', ' ')}
              </Text>
            ))}
            <Text style={styles.adviceTip}>💡 {advice.tip}</Text>
          </View>
        ) : (
          <Text style={styles.loadingText}>Getting suggestions...</Text>
        )}
      </View>
    </SafeAreaView>
  );
};

// ========== ALERTS SCREEN ==========

const AlertsScreen = () => {
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    fetch(`${API_BASE}/alerts`)
      .then(res => res.json())
      .then(setAlerts)
      .catch(console.error);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.screenTitle}>⚠️ Alerts</Text>
      <FlatList
        data={alerts}
        keyExtractor={(item, idx) => idx.toString()}
        renderItem={({ item }) => (
          <View style={[styles.card, { borderLeftColor: item.severity >= 3 ? '#F44336' : '#FF9800', borderLeftWidth: 4 }]}>
            <Text style={styles.alertType}>{item.type}</Text>
            <Text style={styles.alertMessage}>{item.message}</Text>
            <Text style={styles.alertTime}>{item.timestamp}</Text>
          </View>
        )}
      />
    </SafeAreaView>
  );
};

// ========== NAVIGATION ==========

const Tab = createBottomTabNavigator();

const App = () => {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          tabBarActiveTintColor: '#4CAF50',
          tabBarStyle: { backgroundColor: '#F5F5F5' },
        }}
      >
        <Tab.Screen name="Garden" component={GardenDashboard} options={{ tabBarLabel: '🌿 Garden' }} />
        <Tab.Screen name="Weather" component={WeatherScreen} options={{ tabBarLabel: '🌤️ Weather' }} />
        <Tab.Screen name="Harvest" component={HarvestCalendar} options={{ tabBarLabel: '🍅 Harvest' }} />
        <Tab.Screen name="Planting" component={PlantingAdvisor} options={{ tabBarLabel: '🌱 Plant' }} />
        <Tab.Screen name="Alerts" component={AlertsScreen} options={{ tabBarLabel: '⚠️ Alerts' }} />
      </Tab.Navigator>
    </NavigationContainer>
  );
};

// ========== STYLES ==========

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#FAFAFA',
    padding: 12,
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  loadingText: {
    fontSize: 16,
    color: '#666',
    textAlign: 'center',
    padding: 20,
  },
  card: {
    backgroundColor: '#FFFFFF',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.08,
    shadowRadius: 4,
    elevation: 2,
  },
  cardTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 12,
    color: '#333',
  },
  row: {
    flexDirection: 'row',
    justifyContent: 'space-around',
  },
  statBox: {
    alignItems: 'center',
    padding: 8,
  },
  statValue: {
    fontSize: 28,
    fontWeight: '700',
    color: '#333',
  },
  statLabel: {
    fontSize: 12,
    color: '#888',
    marginTop: 4,
  },
  plantCard: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 14,
    marginBottom: 10,
    borderLeftWidth: 4,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.06,
    shadowRadius: 2,
    elevation: 1,
  },
  plantHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 8,
  },
  plantEmoji: {
    fontSize: 28,
    marginRight: 12,
  },
  plantInfo: {
    flex: 1,
  },
  plantName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  plantType: {
    fontSize: 13,
    color: '#888',
    textTransform: 'capitalize',
  },
  healthBadge: {
    borderRadius: 12,
    paddingHorizontal: 10,
    paddingVertical: 4,
  },
  healthBadgeText: {
    color: '#FFF',
    fontWeight: '600',
    fontSize: 14,
  },
  plantMetrics: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  metricText: {
    fontSize: 14,
    color: '#555',
  },
  plantList: {
    paddingBottom: 20,
  },
  screenTitle: {
    fontSize: 22,
    fontWeight: '700',
    color: '#333',
    marginBottom: 16,
  },
  weatherTemp: {
    fontSize: 48,
    fontWeight: '300',
    color: '#333',
    marginBottom: 8,
  },
  weatherDetail: {
    fontSize: 15,
    color: '#555',
    marginBottom: 4,
  },
  rainAlert: {
    backgroundColor: '#E3F2FD',
    borderRadius: 8,
    padding: 12,
    marginTop: 12,
  },
  rainAlertText: {
    fontSize: 14,
    color: '#1565C0',
  },
  adviceItem: {
    fontSize: 16,
    color: '#333',
    marginBottom: 8,
    paddingLeft: 4,
  },
  adviceTip: {
    fontSize: 14,
    color: '#666',
    fontStyle: 'italic',
    marginTop: 8,
    paddingTop: 8,
    borderTopWidth: 1,
    borderTopColor: '#EEE',
  },
  alertType: {
    fontSize: 15,
    fontWeight: '600',
    color: '#333',
    marginBottom: 4,
  },
  alertMessage: {
    fontSize: 14,
    color: '#555',
    marginBottom: 4,
  },
  alertTime: {
    fontSize: 12,
    color: '#999',
  },
});

export default App;