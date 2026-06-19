// ActivityScreen — Activity timeline + trends

import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, RefreshControl } from 'react-native';
import { getActivity, ActivityEntry } from '../api';

const PET_ID = 1;

const ACTIVITY_ICONS: { [key: string]: string } = {
  resting: '😴', walking: '🚶', running: '🏃', sleeping: '💤',
  scratching: '🐕', head_shaking: '🐾', licking: '👅', eating: '🍽️', playing: '🎾',
};

export default function ActivityScreen() {
  const [activities, setActivities] = useState<ActivityEntry[]>([]);
  const [refreshing, setRefreshing] = useState(false);

  const fetchActivity = useCallback(async () => {
    try {
      const data = await getActivity(PET_ID);
      setActivities(data);
    } catch (e) { console.error(e); }
  }, []);

  useEffect(() => {
    fetchActivity();
    const interval = setInterval(fetchActivity, 30000);
    return () => clearInterval(interval);
  }, [fetchActivity]);

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchActivity();
    setRefreshing(false);
  };

  // Summarize activity distribution
  const summary = activities.reduce((acc, a) => {
    acc[a.activity] = (acc[a.activity] || 0) + 1;
    return acc;
  }, {} as { [key: string]: number });

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Activity Timeline</Text>

      <View style={styles.summaryCard}>
        <Text style={styles.summaryTitle}>Today's Summary</Text>
        {Object.entries(summary).map(([activity, count]) => (
          <View key={activity} style={styles.summaryRow}>
            <Text style={styles.summaryIcon}>{ACTIVITY_ICONS[activity] || '❓'}</Text>
            <Text style={styles.summaryActivity}>{activity}</Text>
            <Text style={styles.summaryCount}>{count} min</Text>
          </View>
        ))}
      </View>

      <FlatList
        data={activities.slice(-50).reverse()}
        keyExtractor={(_, i) => i.toString()}
        refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
        renderItem={({ item }) => (
          <View style={styles.activityRow}>
            <Text style={styles.activityIcon}>{ACTIVITY_ICONS[item.activity] || '❓'}</Text>
            <View style={styles.activityInfo}>
              <Text style={styles.activityName}>{item.activity}</Text>
              <Text style={styles.activityTime}>{new Date(item.ts).toLocaleTimeString()}</Text>
            </View>
            <View style={styles.vitals}>
              <Text style={styles.vitalText}>HR {item.hr}</Text>
              <Text style={styles.vitalText}>HRV {item.hrv.toFixed(0)}ms</Text>
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
  summaryCard: { backgroundColor: 'white', margin: 16, padding: 16, borderRadius: 12 },
  summaryTitle: { fontSize: 16, fontWeight: '600', marginBottom: 12 },
  summaryRow: { flexDirection: 'row', alignItems: 'center', marginBottom: 8 },
  summaryIcon: { fontSize: 20, marginRight: 12 },
  summaryActivity: { flex: 1, fontSize: 14, textTransform: 'capitalize' },
  summaryCount: { fontSize: 14, color: '#666' },
  activityRow: { flexDirection: 'row', alignItems: 'center', backgroundColor: 'white', marginHorizontal: 16, marginBottom: 4, padding: 12, borderRadius: 8 },
  activityIcon: { fontSize: 24, marginRight: 12 },
  activityInfo: { flex: 1 },
  activityName: { fontSize: 14, fontWeight: '500', textTransform: 'capitalize' },
  activityTime: { fontSize: 12, color: '#999' },
  vitals: { alignItems: 'flex-end' },
  vitalText: { fontSize: 11, color: '#666' },
});