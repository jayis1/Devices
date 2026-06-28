/**
 * TimelineScreen — Activity & treatment history
 */
import React, { useState } from 'react';
import { View, Text, ScrollView, StyleSheet } from 'react-native';

const mockTimeline = [
  { date: '2024-06-28 02:14', type: 'detection', title: 'House Mouse detected', detail: 'Kitchen · 82% confidence', icon: '🐭' },
  { date: '2024-06-28 01:00', type: 'deterrent', title: 'Ultrasonic activated', detail: 'Kitchen · 25 kHz sweep', icon: '🔊' },
  { date: '2024-06-27 22:30', type: 'trap', title: 'Kitchen trap triggered', detail: 'Mouse · 22g catch', icon: '🎯' },
  { date: '2024-06-27 21:00', type: 'detection', title: 'German Cockroach detected', detail: 'Kitchen · 71% confidence', icon: '🪳' },
  { date: '2024-06-27 09:00', type: 'alert', title: 'Termite swarm season approaching', detail: 'Your area: termite risk in 14 days', icon: '🚨' },
  { date: '2024-06-26 20:00', type: 'deterrent', title: 'Peppermint oil dose', detail: 'Kitchen diffuser · dose #12', icon: '🌿' },
  { date: '2024-06-26 03:00', type: 'detection', title: 'House Mouse detected', detail: 'Garage · 88% confidence', icon: '🐭' },
];

export default function TimelineScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Timeline</Text>
      {mockTimeline.map((item, i) => (
        <View key={i} style={styles.timelineItem}>
          <View style={styles.timelineDot}>
            <Text style={styles.dotIcon}>{item.icon}</Text>
          </View>
          <View style={styles.timelineContent}>
            <Text style={styles.itemDate}>{item.date}</Text>
            <Text style={styles.itemTitle}>{item.title}</Text>
            <Text style={styles.itemDetail}>{item.detail}</Text>
          </View>
        </View>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e' },
  title: { color: '#fff', fontSize: 22, fontWeight: 'bold', padding: 15 },
  timelineItem: { flexDirection: 'row', paddingHorizontal: 15, marginBottom: 15 },
  timelineDot: { width: 40, height: 40, borderRadius: 20, backgroundColor: '#16213e', justifyContent: 'center', alignItems: 'center', marginRight: 15 },
  dotIcon: { fontSize: 18 },
  timelineContent: { flex: 1, backgroundColor: '#16213e', padding: 12, borderRadius: 10 },
  itemDate: { color: '#7f8c8d', fontSize: 11 },
  itemTitle: { color: '#fff', fontSize: 15, fontWeight: 'bold', marginTop: 2 },
  itemDetail: { color: '#95a5a6', fontSize: 12, marginTop: 2 },
});