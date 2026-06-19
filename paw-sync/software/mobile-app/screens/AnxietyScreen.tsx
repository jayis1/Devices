// AnxietyScreen — Separation anxiety episodes + enrichment history

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, RefreshControl } from 'react-native';
import { getAnxiety, AnxietyEpisode } from '../api';

const PET_ID = 1;

export default function AnxietyScreen() {
  const [episodes, setEpisodes] = useState<AnxietyEpisode[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchAnxiety = useCallback(async () => {
    try {
      const data = await getAnxiety(PET_ID);
      setEpisodes(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => {
    fetchAnxiety();
    const interval = setInterval(fetchAnxiety, 60000);
    return () => clearInterval(interval);
  }, [fetchAnxiety]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchAnxiety();
    setRefreshing(false);
  };

  const totalEpisodes = episodes.length;
  const avgDuration = totalEpisodes > 0
    ? Math.round(episodes.reduce((s, e) => s + e.duration_s, 0) / totalEpisodes / 60)
    : 0;
  const lastWeek = episodes.filter(e =>
    new Date(e.ts) > new Date(Date.now() - 7 * 24 * 60 * 60 * 1000)
  ).length;

  return (
    <View style={styles.container}>
      <Text style={styles.header}>💜 Anxiety Monitor</Text>

      <View style={styles.statsCard}>
        <View style={styles.statItem}>
          <Text style={styles.statValue}>{lastWeek}</Text>
          <Text style={styles.statLabel}>Episodes (7d)</Text>
        </View>
        <View style={styles.statItem}>
          <Text style={styles.statValue}>{avgDuration}m</Text>
          <Text style={styles.statLabel}>Avg Duration</Text>
        </View>
        <View style={styles.statItem}>
          <Text style={[styles.statValue, { color: lastWeek > 5 ? '#F44336' : '#4CAF50' }]}>
            {lastWeek > 5 ? 'High' : lastWeek > 2 ? 'Moderate' : 'Low'}
          </Text>
          <Text style={styles.statLabel}>Severity</Text>
        </View>
      </View>

      <Text style={styles.sectionTitle}>Recent Episodes</Text>

      <FlatList
        data={episodes}
        keyExtractor={(_, i) => i.toString()}
        refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
        renderItem={({ item }) => (
          <View style={styles.episodeRow}>
            <View style={styles.episodeInfo}>
              <Text style={styles.episodeTime}>{new Date(item.ts).toLocaleString()}</Text>
              <Text style={styles.episodeDetail}>
                {item.behavior} · {Math.round(item.duration_s / 60)}min
              </Text>
              {item.vocalization > 0 && (
                <Text style={styles.vocalType}>
                  Vocal: {['', 'pain', 'anxiety', 'alert', 'play', 'attention', 'distress'][item.vocalization]}
                </Text>
              )}
            </View>
          </View>
        )}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  header: { fontSize: 22, fontWeight: 'bold', textAlign: 'center', marginVertical: 16 },
  statsCard: { flexDirection: 'row', justifyContent: 'space-around', backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  statItem: { alignItems: 'center' },
  statValue: { fontSize: 24, fontWeight: 'bold', color: '#9C27B0' },
  statLabel: { fontSize: 11, color: '#666', marginTop: 4 },
  sectionTitle: { fontSize: 16, fontWeight: '600', marginHorizontal: 16, marginBottom: 8 },
  episodeRow: { backgroundColor: 'white', marginHorizontal: 16, marginBottom: 4, padding: 12, borderRadius: 8, borderLeftWidth: 4, borderLeftColor: '#9C27B0' },
  episodeInfo: { flex: 1 },
  episodeTime: { fontSize: 12, color: '#999' },
  episodeDetail: { fontSize: 14, fontWeight: '500', marginVertical: 2, textTransform: 'capitalize' },
  vocalType: { fontSize: 11, color: '#666' },
});