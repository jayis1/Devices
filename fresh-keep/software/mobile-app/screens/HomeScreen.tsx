/**
 * FreshKeep — Home Screen
 * 
 * Dashboard overview: system status, fridge/pantry summary,
 * fire safety status, expiring items, and alerts.
 */

import React, { useState, useEffect } from 'react';
import { View, ScrollView, StyleSheet, RefreshControl } from 'react-native';
import { Card, Title, Paragraph, Badge, IconButton, ProgressBar, Text } from 'react-native-paper';
import { MqttService } from '../services/mqtt';

interface SystemStatus {
  fridge: {
    spoilage_score: number;
    temp_c: number;
    door: number;
    battery_pct: number;
  } | null;
  pantry: {
    items_count: number;
    temp_c: number;
    door: number;
  } | null;
  stove_guard: {
    alert_level: number;
    max_temp_c: number;
    gas_valve_open: number;
    fire_confidence: number;
  } | null;
  fire_alerts: number;
}

export default function HomeScreen({ navigation }) {
  const [status, setStatus] = useState<SystemStatus | null>(null);
  const [refreshing, setRefreshing] = useState(false);
  const [alerts, setAlerts] = useState<string[]>([]);

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 5000); // Poll every 5s
    return () => clearInterval(interval);
  }, []);

  const fetchStatus = async () => {
    try {
      const response = await fetch('http://freshkeep.local:8000/api/status');
      const data = await response.json();
      setStatus(data);
      
      // Generate alerts
      const newAlerts: string[] = [];
      if (data.fridge?.spoilage_score > 80) {
        newAlerts.push('⚠️ Fridge: Food spoilage detected! Check items immediately.');
      }
      if (data.stove_guard?.alert_level >= 4) {
        newAlerts.push('🔥 FIRE ALARM! Stove guard has detected a fire hazard!');
      } else if (data.stove_guard?.alert_level >= 3) {
        newAlerts.push('⚠️ Stove: Unattended cooking detected.');
      }
      if (data.fire_alerts > 0) {
        newAlerts.push(`🔥 ${data.fire_alerts} unresolved fire alarm(s)`);
      }
      setAlerts(newAlerts);
    } catch (error) {
      console.error('Failed to fetch status:', error);
      setAlerts(['❌ Cannot connect to FreshKeep hub']);
    }
    setRefreshing(false);
  };

  const getSpoilageColor = (score: number) => {
    if (score > 80) return '#F44336'; // Red
    if (score > 50) return '#FF9800'; // Orange
    if (score > 30) return '#FFC107'; // Yellow
    return '#4CAF50'; // Green
  };

  const getAlertLevelColor = (level: number) => {
    switch (level) {
      case 0: return '#4CAF50'; // Safe
      case 1: return '#8BC34A'; // Info
      case 2: return '#FFC107'; // Warning
      case 3: return '#FF9800'; // Urgent
      case 4: return '#F44336'; // Critical
      default: return '#9E9E9E';
    }
  };

  return (
    <ScrollView 
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={() => { setRefreshing(true); fetchStatus(); }} />}
    >
      {/* Alerts */}
      {alerts.map((alert, i) => (
        <Card key={i} style={[styles.alertCard, alert.includes('FIRE') && styles.fireAlert]}>
          <Card.Content>
            <Paragraph style={styles.alertText}>{alert}</Paragraph>
          </Card.Content>
        </Card>
      ))}

      {/* Fridge Status */}
      <Card style={styles.card}>
        <Card.Title 
          title="Fridge" 
          left={(props) => <Icon name="fridge" {...props} />}
          right={(props) => status?.fridge && (
            <Badge>{status.fridge.battery_pct}%</Badge>
          )}
        />
        <Card.Content>
          {status?.fridge ? (
            <>
              <View style={styles.row}>
                <Text>Temperature: {status.fridge.temp_c.toFixed(1)}°C</Text>
                <Text style={{ color: status.fridge.door ? '#FF9800' : '#4CAF50' }}>
                  {status.fridge.door ? 'Door OPEN' : 'Door closed'}
                </Text>
              </View>
              <View style={styles.row}>
                <Text>Freshness Score:</Text>
                <ProgressBar 
                  progress={status.fridge.spoilage_score / 100}
                  color={getSpoilageColor(status.fridge.spoilage_score)}
                  style={styles.progressBar}
                />
              </View>
              <Text style={{ color: getSpoilageColor(status.fridge.spoilage_score) }}>
                {status.fridge.spoilage_score > 80 ? '⚠️ Check items!' : 
                 status.fridge.spoilage_score > 50 ? 'Some items expiring' : 
                 '✅ All fresh'}
              </Text>
            </>
          ) : (
            <Paragraph>Connecting to fridge node...</Paragraph>
          )}
        </Card.Content>
      </Card>

      {/* Pantry Status */}
      <Card style={styles.card}>
        <Card.Title 
          title="Pantry" 
          left={(props) => <Icon name="cupboard" {...props} />}
        />
        <Card.Content>
          {status?.pantry ? (
            <>
              <Text>Items tracked: {status.pantry.items_count}</Text>
              <Text>Temperature: {status.pantry.temp_c.toFixed(1)}°C</Text>
              <Text style={{ color: status.pantry.door ? '#FF9800' : '#4CAF50' }}>
                {status.pantry.door ? 'Door OPEN' : 'Door closed'}
              </Text>
            </>
          ) : (
            <Paragraph>Connecting to pantry node...</Paragraph>
          )}
        </Card.Content>
      </Card>

      {/* Stove Guard Status */}
      <Card style={[styles.card, status?.stove_guard?.alert_level && status.stove_guard.alert_level >= 3 && styles.fireAlertCard]}>
        <Card.Title 
          title="Stove Guard" 
          left={(props) => <Icon name="shield-home" {...props} />}
          right={(props) => status?.stove_guard && (
            <Badge style={{ backgroundColor: getAlertLevelColor(status.stove_guard.alert_level) }}>
              {['Safe', 'Info', 'Warning', 'Urgent', 'FIRE!'][status.stove_guard.alert_level]}
            </Badge>
          )}
        />
        <Card.Content>
          {status?.stove_guard ? (
            <>
              <Text>Max surface temp: {status.stove_guard.max_temp_c}°C</Text>
              <Text>Gas valve: {status.stove_guard.gas_valve_open ? 'Open' : 'CLOSED (safe)'}</Text>
              <Text>Fire confidence: {status.stove_guard.fire_confidence}%</Text>
              <Text style={{ color: getAlertLevelColor(status.stove_guard.alert_level), fontWeight: 'bold' }}>
                {status.stove_guard.alert_level >= 4 ? '🚨 FIRE DETECTED — Evacuate!' :
                 status.stove_guard.alert_level >= 3 ? '⚠️ Stove shutoff activated' :
                 status.stove_guard.alert_level >= 2 ? '⚡ Unattended cooking warning' :
                 status.stove_guard.alert_level >= 1 ? 'ℹ️ Cooking detected' :
                 '✅ All clear'}
              </Text>
            </>
          ) : (
            <Paragraph>Connecting to stove guard...</Paragraph>
          )}
        </Card.Content>
      </Card>

      {/* Quick Actions */}
      <Card style={styles.card}>
        <Card.Title title="Quick Actions" />
        <Card.Content style={styles.actions}>
          <IconButton icon="barcode-scan" mode="contained" onPress={() => navigation.navigate('Inventory')} />
          <IconButton icon="cart-plus" mode="contained" onPress={() => navigation.navigate('Shopping')} />
          <IconButton icon="chef-hat" mode="contained" onPress={() => navigation.navigate('Recipes')} />
          <IconButton icon="fire-alert" mode="contained" onPress={() => navigation.navigate('Fire Safety')} />
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 8 },
  card: { marginBottom: 12 },
  alertCard: { marginBottom: 8, backgroundColor: '#FFF3E0' },
  fireAlert: { backgroundColor: '#FFCDD2' },
  fireAlertCard: { borderWidth: 2, borderColor: '#F44336' },
  alertText: { fontWeight: 'bold' },
  row: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 },
  progressBar: { flex: 1, marginLeft: 8, height: 8, borderRadius: 4 },
  actions: { flexDirection: 'row', justifyContent: 'space-around' },
});