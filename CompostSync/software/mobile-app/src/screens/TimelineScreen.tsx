import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { LineChart } from 'react-native-chart-kit';

const SCREEN_WIDTH = 350;

export default function TimelineScreen() {
  // In production: fetch from API
  const tempData = [15, 18, 25, 45, 58, 62, 60, 50, 42, 35, 28, 25, 22, 21];
  const co2Data = [300, 500, 1200, 3000, 4500, 5000, 4800, 3000, 1500, 800, 500, 400, 350, 320];
  const moistureData = [55, 56, 58, 60, 62, 65, 64, 62, 58, 55, 52, 50, 49, 48];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Timeline</Text>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>🌡️ Temperature (°C)</Text>
        <LineChart
          data={{ labels: ['D1','D5','D10','D15','D20','D25','D30'], datasets: [{ data: tempData }] }}
          width={SCREEN_WIDTH - 32}
          height={180}
          chartConfig={chartConfig}
          bezier
          style={styles.chart}
        />
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>🫧 CO₂ (ppm)</Text>
        <LineChart
          data={{ labels: ['D1','D5','D10','D15','D20','D25','D30'], datasets: [{ data: co2Data }] }}
          width={SCREEN_WIDTH - 32}
          height={180}
          chartConfig={chartConfig}
          bezier
          style={styles.chart}
        />
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>💧 Moisture (%)</Text>
        <LineChart
          data={{ labels: ['D1','D5','D10','D15','D20','D25','D30'], datasets: [{ data: moistureData }] }}
          width={SCREEN_WIDTH - 32}
          height={180}
          chartConfig={chartConfig}
          bezier
          style={styles.chart}
        />
      </View>

      <View style={styles.achievements}>
        <Text style={styles.achievementsTitle}>🏆 Achievements</Text>
        <View style={styles.badgeRow}>
          <View style={styles.badge}><Text style={styles.badgeIcon}>🔥</Text><Text style={styles.badgeText}>First Hot Phase</Text></View>
          <View style={styles.badge}><Text style={styles.badgeIcon}>♻️</Text><Text style={styles.badgeText}>50 kg Diverted</Text></View>
          <View style={styles.badge}><Text style={styles.badgeIcon}>🌱</Text><Text style={styles.badgeText}>Black Gold!</Text></View>
        </View>
      </View>
    </ScrollView>
  );
}

const chartConfig = {
  backgroundGradientFrom: '#1E1E1E',
  backgroundGradientTo: '#1E1E1E',
  color: (opacity = 1) => `rgba(76, 175, 80, ${opacity})`,
  labelColor: (opacity = 1) => `rgba(255, 255, 255, ${0.5})`,
  strokeWidth: 2,
};

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#121212', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  card: { backgroundColor: '#1E1E1E', borderRadius: 12, padding: 16, marginBottom: 12 },
  cardTitle: { fontSize: 16, color: '#fff', marginBottom: 8 },
  chart: { borderRadius: 8 },
  achievements: { marginTop: 12, marginBottom: 20 },
  achievementsTitle: { fontSize: 18, fontWeight: 'bold', color: '#fff', marginBottom: 12 },
  badgeRow: { flexDirection: 'row', justifyContent: 'space-around' },
  badge: { alignItems: 'center', padding: 12 },
  badgeIcon: { fontSize: 36 },
  badgeText: { fontSize: 12, color: '#888', marginTop: 4, textAlign: 'center' },
});