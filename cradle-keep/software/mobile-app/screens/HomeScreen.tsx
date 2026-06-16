/**
 * CradleKeep — Home Screen
 * 
 * Real-time dashboard showing baby's current state:
 * - Sleep stage icon + label
 * - Breathing rate with animation
 * - Last feeding time + volume
 * - Room conditions (temp, humidity, noise, light)
 * - Current alerts
 * - Quick action buttons (play sound, start warming)
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Animated } from 'react-native';
import { Card, Title, Paragraph, IconButton, ProgressBar, Badge } from 'react-native-paper';

// Cry type labels
const CRY_LABELS = ['No cry', 'Hungry', 'Tired', 'Pain', 'Colic', 'Discomfort'];
const SLEEP_LABELS = ['Awake', 'Light Sleep', 'Deep Sleep', 'REM Sleep'];
const SLEEP_ICONS = ['eye-outline', 'weather-night-partly-cloudy', 'moon-waning-crescent', 'brain'];
const SLEEP_COLORS = ['#FF9800', '#2196F3', '#1A237E', '#9C27B0'];

// Alert level colors
const ALERT_COLORS = ['#4CAF50', '#8BC34A', '#FFC107', '#FF5722', '#F44336'];

export default function HomeScreen({ navigation }) {
  const [breathRate, setBreathRate] = useState(32);
  const [sleepStage, setSleepStage] = useState(2); // SLEEP_DEEP
  const [cryType, setCryType] = useState(0); // CRY_NONE
  const [cryConfidence, setCryConfidence] = useState(0);
  const [roomTemp, setRoomTemp] = useState(22.3);
  const [humidity, setHumidity] = useState(52);
  const [co2, setCo2] = useState(420);
  const [noise, setNoise] = useState(25);
  const [light, setLight] = useState(2);
  const [lastFeed, setLastFeed] = useState('2h 15m ago');
  const [lastFeedVol, setLastFeedVol] = useState(95);
  const [alertLevel, setAlertLevel] = useState(0);
  const [wetAlert, setWetAlert] = useState(false);
  
  // Breathing animation
  const breathAnim = useState(new Animated.Value(1))[0];
  
  useEffect(() => {
    // Animate breathing circle
    const animateBreath = () => {
      Animated.sequence([
        Animated.timing(breathAnim, { toValue: 1.15, duration: 1500, useNativeDriver: true }),
        Animated.timing(breathAnim, { toValue: 1.0, duration: 1500, useNativeDriver: true }),
      ]).start(() => animateBreath());
    };
    animateBreath();
    
    // TODO: Connect to BLE/MQTT service for real data
  }, []);

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {/* Alert Banner */}
      {alertLevel > 0 && (
        <Card style={[styles.alertCard, { borderLeftColor: ALERT_COLORS[alertLevel] }]}>
          <Card.Content>
            <Text style={styles.alertText}>
              {alertLevel === 1 && '⚠️ Breathing slightly irregular'}
              {alertLevel === 2 && '🟡 Breathing pause detected — check baby'}
              {alertLevel === 3 && '🟠 Prolonged breathing pause — please check immediately'}
              {alertLevel === 4 && '🔴 BREATHING ALERT — EMERGENCY'}
            </Text>
          </Card.Content>
        </Card>
      )}

      {/* Sleep Stage Card */}
      <Card style={styles.card}>
        <Card.Content style={styles.sleepCard}>
          <View style={styles.sleepInfo}>
            <IconButton icon={SLEEP_ICONS[sleepStage]} size={48} color={SLEEP_COLORS[sleepStage]} />
            <View>
              <Title style={styles.sleepLabel}>{SLEEP_LABELS[sleepStage]}</Title>
              <Paragraph style={styles.sleepSubtext}>Since 8:45 PM • 1h 22m</Paragraph>
            </View>
          </View>
          <ProgressBar progress={0.6} color={SLEEP_COLORS[sleepStage]} style={styles.sleepProgress} />
        </Card.Content>
      </Card>

      {/* Breathing Rate Card */}
      <Card style={styles.card}>
        <Card.Content style={styles.breathCard}>
          <Text style={styles.sectionTitle}>Breathing</Text>
          <View style={styles.breathCircle}>
            <Animated.View style={[styles.breathCircleInner, { transform: [{ scale: breathAnim }] }]}>
              <Text style={styles.breathRate}>{breathRate}</Text>
              <Text style={styles.breathUnit}>BPM</Text>
            </Animated.View>
          </View>
          <Text style={styles.breathStatus}>
            {breathRate >= 15 && breathRate <= 70 ? '✅ Normal breathing' : '⚠️ Irregular breathing'}
          </Text>
          {wetAlert && (
            <View style={styles.wetAlert}>
              <Text style={styles.wetAlertText}>💧 Moisture detected — possible diaper leak</Text>
            </View>
          )}
        </Card.Content>
      </Card>

      {/* Cry Monitor Card */}
      {cryType > 0 && (
        <Card style={styles.card}>
          <Card.Content>
            <Text style={styles.sectionTitle}>Cry Detected</Text>
            <View style={styles.cryRow}>
              <Text style={styles.cryType}>{CRY_LABELS[cryType]}</Text>
              <Text style={styles.cryConfidence}>{Math.round(cryConfidence * 100 / 255)}% confidence</Text>
            </View>
            <Text style={styles.crySuggestion}>
              {cryType === 1 && '🍼 Time to feed? Last feeding was 2h 15m ago.'}
              {cryType === 2 && '😴 Baby seems tired — try white noise or gentle rocking.'}
              {cryType === 3 && '😰 Pain cry detected — check for discomfort, fever, or injury.'}
              {cryType === 4 && '😢 Colic cry — try shushing, gentle tummy massage, or swaddling.'}
              {cryType === 5 && '😤 General discomfort — check diaper, temperature, or clothing.'}
            </Text>
          </Card.Content>
        </Card>
      )}

      {/* Last Feeding Card */}
      <Card style={styles.card}>
        <Card.Content>
          <Text style={styles.sectionTitle}>Last Feeding</Text>
          <View style={styles.feedingRow}>
            <View style={styles.feedingItem}>
              <Text style={styles.feedingValue}>{lastFeedVol}ml</Text>
              <Text style={styles.feedingLabel}>Volume</Text>
            </View>
            <View style={styles.feedingItem}>
              <Text style={styles.feedingValue}>{lastFeed}</Text>
              <Text style={styles.feedingLabel}>Time</Text>
            </View>
            <View style={styles.feedingItem}>
              <Text style={styles.feedingValue}>37°C</Text>
              <Text style={styles.feedingLabel}>Temp</Text>
            </View>
          </View>
        </Card.Content>
      </Card>

      {/* Room Conditions Card */}
      <Card style={styles.card}>
        <Card.Content>
          <Text style={styles.sectionTitle}>Nursery Conditions</Text>
          <View style={styles.conditionsGrid}>
            <View style={styles.conditionItem}>
              <Text style={styles.conditionIcon}>🌡️</Text>
              <Text style={styles.conditionValue}>{roomTemp}°C</Text>
              <Text style={styles.conditionLabel}>Temperature</Text>
            </View>
            <View style={styles.conditionItem}>
              <Text style={styles.conditionIcon}>💧</Text>
              <Text style={styles.conditionValue}>{humidity}%</Text>
              <Text style={styles.conditionLabel}>Humidity</Text>
            </View>
            <View style={styles.conditionItem}>
              <Text style={styles.conditionIcon}>🔊</Text>
              <Text style={styles.conditionValue}>{noise} dB</Text>
              <Text style={styles.conditionLabel}>Noise</Text>
            </View>
            <View style={styles.conditionItem}>
              <Text style={styles.conditionIcon}>💡</Text>
              <Text style={styles.conditionValue}>{light} lux</Text>
              <Text style={styles.conditionLabel}>Light</Text>
            </View>
          </View>
        </Card.Content>
      </Card>

      {/* Quick Actions */}
      <View style={styles.actionsRow}>
        <TouchableOpacity style={styles.actionButton} onPress={() => navigation.navigate('Cry')}>
          <IconButton icon="ear-hearing" size={24} color="#4CAF50" />
          <Text style={styles.actionLabel}>Soothe</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.actionButton} onPress={() => navigation.navigate('Feeding')}>
          <IconButton icon="baby-bottle" size={24} color="#2196F3" />
          <Text style={styles.actionLabel}>Warm</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.actionButton} onPress={() => {}}>
          <IconButton icon="video-outline" size={24} color="#9C27B0" />
          <Text style={styles.actionLabel}>Camera</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.actionButton} onPress={() => navigation.navigate('BreathingDetail')}>
          <IconButton icon="heart-pulse" size={24} color="#F44336" />
          <Text style={styles.actionLabel}>Breathing</Text>
        </TouchableOpacity>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#F5F5F5' },
  content: { padding: 16 },
  card: { marginBottom: 12, borderRadius: 12 },
  alertCard: { marginBottom: 12, borderLeftWidth: 4, borderRadius: 12 },
  alertText: { fontSize: 14, fontWeight: '600' },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 8, color: '#333' },
  sleepCard: { alignItems: 'flex-start' },
  sleepInfo: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
  sleepLabel: { fontSize: 20, fontWeight: 'bold', color: SLEEP_COLORS[2] },
  sleepSubtext: { fontSize: 12, color: '#666' },
  sleepProgress: { width: '100%', marginTop: 4 },
  breathCard: { alignItems: 'center' },
  breathCircle: { width: 160, height: 160, borderRadius: 80, backgroundColor: '#E8F5E9', 
                  justifyContent: 'center', alignItems: 'center', marginVertical: 12 },
  breathCircleInner: { justifyContent: 'center', alignItems: 'center' },
  breathRate: { fontSize: 48, fontWeight: 'bold', color: '#4CAF50' },
  breathUnit: { fontSize: 14, color: '#666' },
  breathStatus: { fontSize: 14, color: '#666', marginTop: 4 },
  wetAlert: { backgroundColor: '#FFF3E0', padding: 8, borderRadius: 8, marginTop: 8 },
  wetAlertText: { fontSize: 13, color: '#E65100' },
  cryRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  cryType: { fontSize: 18, fontWeight: 'bold' },
  cryConfidence: { fontSize: 14, color: '#666' },
  crySuggestion: { fontSize: 13, color: '#555', marginTop: 4, lineHeight: 18 },
  feedingRow: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 8 },
  feedingItem: { alignItems: 'center' },
  feedingValue: { fontSize: 20, fontWeight: 'bold', color: '#2196F3' },
  feedingLabel: { fontSize: 12, color: '#666' },
  conditionsGrid: { flexDirection: 'row', justifyContent: 'space-around', flexWrap: 'wrap' },
  conditionItem: { alignItems: 'center', width: '48%', marginBottom: 8 },
  conditionIcon: { fontSize: 24 },
  conditionValue: { fontSize: 18, fontWeight: 'bold', color: '#333' },
  conditionLabel: { fontSize: 11, color: '#666' },
  actionsRow: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 8, marginBottom: 20 },
  actionButton: { alignItems: 'center', backgroundColor: '#FFF', padding: 8, borderRadius: 12, 
                  elevation: 2, shadowColor: '#000', shadowOpacity: 0.1, shadowRadius: 4 },
  actionLabel: { fontSize: 11, color: '#333', marginTop: 2 },
});