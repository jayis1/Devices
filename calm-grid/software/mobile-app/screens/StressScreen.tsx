// StressScreen — Stress timeline + episode map

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, RefreshControl, FlatList } from 'react-native';
import { getStress, getEpisodes, Episode } from '../api';

const USER_ID = 1;
const ACTIVITY_NAMES = ['sitting', 'walking', 'running', 'resting', 'sleeping', 'working', 'commuting', 'exercising'];

export default function StressScreen() {
  const [trend, setTrend] = useState<any[]>([]);
  const [episodes, setEpisodes] = useState<Episode[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchData = useCallback(async () => {
    try {
      const stressData = await getStress(USER_ID);
      setTrend(stressData.trend);
      const eps = await getEpisodes(USER_ID);
      setEpisodes(eps);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  const renderEpisode = ({ item }: { item: Episode }) => (
    <View style={styles.episodeCard}>
      <Text style={styles.episodeTime}>{new Date(item.ts).toLocaleString()}</Text>
      <Text style={styles.episodeDetail}>HR: {item.hr} bpm | HRV: {item.hrv_ms?.toFixed(1)} ms</Text>
      <Text style={styles.episodeDetail}>EDA SCR: {item.eda_scr?.toFixed(1)}/min | Activity: {ACTIVITY_NAMES[item.activity] || '—'}</Text>
    </View>
  );

  return (
    <ScrollView style={styles.container} refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}>
      <Text style={styles.header}>Stress Timeline</Text>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>14-Day Stress Trend</Text>
        {trend.length > 0 ? (
          <View style={styles.trendBar}>
            {trend.slice(-30).map((t, i) => (
              <View key={i} style={[styles.trendBarSegment, {
                height: `${t.stress}%`,
                backgroundColor: t.stress < 30 ? '#4CAF50' : t.stress < 50 ? '#FFC107' : t.stress < 70 ? '#FF9800' : '#F44336'
              }]} />
            ))}
          </View>
        ) : <Text style={styles.empty}>No data yet</Text>}
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Acute Stress Episodes ({episodes.length})</Text>
        {episodes.length > 0 ? (
          <FlatList data={episodes} renderItem={renderEpisode} keyExtractor={(item, i) => i.toString()} scrollEnabled={false} />
        ) : <Text style={styles.empty}>No episodes detected</Text>}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 24, fontWeight: 'bold', textAlign: 'center', marginTop: 20, color: '#6C63FF' },
  section: { backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', marginBottom: 12 },
  trendBar: { flexDirection: 'row', alignItems: 'flex-end', height: 100, gap: 2 },
  trendBarSegment: { flex: 1, borderRadius: 2 },
  empty: { color: '#999', textAlign: 'center', padding: 20 },
  episodeCard: { padding: 12, borderBottomWidth: 1, borderBottomColor: '#eee' },
  episodeTime: { fontSize: 12, color: '#666', marginBottom: 4 },
  episodeDetail: { fontSize: 13, color: '#333' },
});