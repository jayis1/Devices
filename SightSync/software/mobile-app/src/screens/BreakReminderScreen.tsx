/**
 * SightSync Mobile App — Break Reminder Screen (20-20-20)
 * License: MIT
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, Vibration } from 'react-native';
import { Card, Button, ProgressBar } from 'react-native-paper';

export default function BreakReminderScreen() {
  const [minutesSinceBreak, setMinutesSinceBreak] = useState(0);
  const [breakActive, setBreakActive] = useState(false);
  const [breakCountdown, setBreakCountdown] = useState(20);

  useEffect(() => {
    const interval = setInterval(() => {
      setMinutesSinceBreak((m) => m + 1);
    }, 60000); // update every minute
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    if (minutesSinceBreak >= 20 && !breakActive) {
      Vibration.vibrate([200, 100, 200]);
    }
  }, [minutesSinceBreak, breakActive]);

  const startBreak = () => {
    setBreakActive(true);
    setBreakCountdown(20);
  };

  useEffect(() => {
    if (breakActive && breakCountdown > 0) {
      const timer = setTimeout(() => setBreakCountdown((c) => c - 1), 1000);
      return () => clearTimeout(timer);
    }
    if (breakCountdown === 0 && breakActive) {
      setBreakActive(false);
      setMinutesSinceBreak(0);
    }
  }, [breakActive, breakCountdown]);

  const progress = minutesSinceBreak / 20;
  const overdue = minutesSinceBreak >= 20;

  return (
    <View style={styles.container}>
      <Card style={styles.card}>
        <Card.Title title="20-20-20 Rule" subtitle="Every 20 min, look 20 ft away for 20 s" />
        <Card.Content>
          <Text style={styles.timerValue}>{minutesSinceBreak}</Text>
          <Text style={styles.timerLabel}>minutes since last break</Text>
          <ProgressBar
            progress={Math.min(progress, 1)}
            color={overdue ? '#D32F2F' : '#0066CC'}
            style={styles.progressBar}
          />
          {overdue && (
            <Text style={styles.overdueText}>
              ⚠️ Break overdue! Look at something 20 feet away.
            </Text>
          )}
        </Card.Content>
      </Card>

      {breakActive ? (
        <Card style={styles.card}>
          <Card.Content style={styles.breakActive}>
            <Text style={styles.breakCountdown}>{breakCountdown}</Text>
            <Text style={styles.breakLabel}>seconds remaining</Text>
            <Text style={styles.breakHint}>Look at something 20 feet (6 m) away</Text>
          </Card.Content>
        </Card>
      ) : (
        <Button
          mode="contained"
          onPress={startBreak}
          style={styles.button}
          buttonColor="#0066CC"
        >
          Start 20-Second Break
        </Button>
      )}

      <Card style={styles.card}>
        <Card.Title title="Today's Break Stats" />
        <Card.Content>
          <Text style={styles.statText}>Breaks taken: 7</Text>
          <Text style={styles.statText}>Compliance rate: 65%</Text>
          <Text style={styles.statText}>Average interval: 22 min</Text>
        </Card.Content>
      </Card>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 8 },
  card: { marginVertical: 8, elevation: 2 },
  timerValue: { fontSize: 72, fontWeight: 'bold', textAlign: 'center', color: '#0066CC' },
  timerLabel: { fontSize: 14, textAlign: 'center', color: '#888', marginBottom: 8 },
  progressBar: { height: 8, borderRadius: 4, marginVertical: 8 },
  overdueText: { fontSize: 16, color: '#D32F2F', textAlign: 'center', marginTop: 8 },
  breakActive: { alignItems: 'center', paddingVertical: 24 },
  breakCountdown: { fontSize: 96, fontWeight: 'bold', color: '#43A047' },
  breakLabel: { fontSize: 16, color: '#888' },
  breakHint: { fontSize: 14, color: '#666', marginTop: 8 },
  button: { marginVertical: 8 },
  statText: { fontSize: 14, marginVertical: 4 },
});