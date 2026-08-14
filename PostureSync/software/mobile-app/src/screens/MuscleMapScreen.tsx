/** Muscle Map Screen — bilateral EMG heatmap */
import React, { useContext } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { PostureSyncContext } from '../services/PostureSyncContext';

const CHANNELS = [
  'L Upper Trap', 'R Upper Trap', 'L Erector', 'R Erector',
  'L SCM', 'R SCM', 'L Rectus Abd', 'R Rectus Abd',
];

export default function MuscleMapScreen() {
  const { emgData } = useContext(PostureSyncContext);
  const rms = emgData?.emg_rms || [0, 0, 0, 0, 0, 0, 0, 0];
  const asymmetry = emgData?.asymmetry_pct || 0;

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Muscle Imbalance</Text>
      <Text style={styles.subtitle}>Bilateral EMG Analysis</Text>

      <View style={styles.asymmetryCard}>
        <Text style={styles.asymmetryLabel}>Max Asymmetry</Text>
        <Text style={[styles.asymmetryValue, {
          color: asymmetry > 30 ? '#F44336' : asymmetry > 20 ? '#FF9800' : '#4CAF50'
        }]}>
          {asymmetry}%
        </Text>
        <Text style={styles.asymmetryNote}>
          {asymmetry > 20 ? '⚠️ Significant imbalance detected' : '✅ Balanced'}
        </Text>
      </View>

      <View style={styles.channelsCard}>
        <Text style={styles.channelsTitle}>EMG Channels (RMS)</Text>
        {CHANNELS.map((name, i) => {
          const value = rms[i] || 0;
          const intensity = Math.min(value / 1000, 1);
          return (
            <View key={i} style={styles.channelRow}>
              <Text style={styles.channelName}>{name}</Text>
              <View style={styles.channelBar}>
                <View style={[styles.channelFill, {
                  width: `${intensity * 100}%`,
                  backgroundColor: intensity > 0.7 ? '#F44336' : intensity > 0.4 ? '#FF9800' : '#4CAF50'
                }]} />
              </View>
              <Text style={styles.channelValue}>{value}</Text>
            </View>
          );
        })}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, backgroundColor: '#FAFAFA' },
  title: { fontSize: 24, fontWeight: '700', color: '#212121' },
  subtitle: { fontSize: 14, color: '#757575', marginBottom: 16 },
  asymmetryCard: { alignItems: 'center', padding: 24, backgroundColor: '#FFF', borderRadius: 16, marginBottom: 12, elevation: 2 },
  asymmetryLabel: { fontSize: 14, color: '#757575' },
  asymmetryValue: { fontSize: 48, fontWeight: '800' },
  asymmetryNote: { fontSize: 14, marginTop: 8 },
  channelsCard: { padding: 20, backgroundColor: '#FFF', borderRadius: 16, elevation: 2 },
  channelsTitle: { fontSize: 16, fontWeight: '700', marginBottom: 12 },
  channelRow: { flexDirection: 'row', alignItems: 'center', marginBottom: 10 },
  channelName: { width: 120, fontSize: 13, color: '#424242' },
  channelBar: { flex: 1, height: 12, backgroundColor: '#E0E0E0', borderRadius: 6, marginHorizontal: 8 },
  channelFill: { height: 12, borderRadius: 6 },
  channelValue: { width: 50, fontSize: 12, textAlign: 'right', color: '#616161' },
});