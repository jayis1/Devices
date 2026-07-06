/**
 * SightSync Mobile App — Smart Lamp Control Screen
 * License: MIT
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { Card, Button, Slider, SegmentedButtons, Switch } from 'react-native-paper';
import api from '../api/client';

export default function LampControlScreen() {
  const [autoMode, setAutoMode] = useState(true);
  const [cct, setCct] = useState(4000);
  const [brightness, setBrightness] = useState(60);
  const [policy, setPolicy] = useState({ schedule: [] });

  useEffect(() => {
    api.getLampPolicy().then((res) => setPolicy(res.data)).catch(console.error);
  }, []);

  const sendOverride = () => {
    api.setLampOverride(cct, brightness, 30).catch(console.error);
  };

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Title title="Smart Lamp Control" subtitle="Circadian-aware adaptive lighting" />
        <Card.Content>
          <View style={styles.modeRow}>
            <Text style={styles.modeLabel}>Auto Mode</Text>
            <Switch value={autoMode} onValueChange={setAutoMode} />
          </View>
        </Card.Content>
      </Card>

      {!autoMode && (
        <Card style={styles.card}>
          <Card.Title title="Manual Override" />
          <Card.Content>
            <Text style={styles.sliderLabel}>Color Temperature: {cct} K</Text>
            <Slider
              value={cct}
              onValueChange={setCct}
              minimumValue={1800}
              maximumValue={6500}
              step={100}
              style={styles.slider}
            />
            <View style={styles.cctLabels}>
              <Text style={styles.cctLabel}>Warm</Text>
              <Text style={styles.cctLabel}>Cool</Text>
            </View>

            <Text style={styles.sliderLabel}>Brightness: {brightness}%</Text>
            <Slider
              value={brightness}
              onValueChange={setBrightness}
              minimumValue={0}
              maximumValue={100}
              step={5}
              style={styles.slider}
            />

            <Button mode="contained" onPress={sendOverride} style={styles.button}
                    buttonColor="#0066CC">
              Apply Override (30 min)
            </Button>
          </Card.Content>
        </Card>
      )}

      <Card style={styles.card}>
        <Card.Title title="Circadian Schedule" />
        <Card.Content>
          {policy.schedule && policy.schedule.map((item: any, i: number) => (
            <View key={i} style={styles.scheduleRow}>
              <Text style={styles.scheduleTime}>{item.hour}:00</Text>
              <Text style={styles.scheduleCct}>{item.cct} K</Text>
              <Text style={styles.scheduleBright}>{item.brightness}%</Text>
            </View>
          ))}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 8 },
  card: { marginVertical: 8, elevation: 2 },
  modeRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  modeLabel: { fontSize: 16 },
  sliderLabel: { fontSize: 14, marginVertical: 8, color: '#333' },
  slider: { width: '100%', height: 40 },
  cctLabels: { flexDirection: 'row', justifyContent: 'space-between' },
  cctLabel: { fontSize: 12, color: '#888' },
  button: { marginTop: 16 },
  scheduleRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8 },
  scheduleTime: { fontSize: 16, fontWeight: 'bold' },
  scheduleCct: { fontSize: 14, color: '#666' },
  scheduleBright: { fontSize: 14, color: '#666' },
});