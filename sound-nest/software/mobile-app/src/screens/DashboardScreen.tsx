/**
 * SoundNest Mobile App — Dashboard Screen
 * React Native + TypeScript
 *
 * Main dashboard showing real-time acoustic overview:
 * current SPL per room, active alerts, masking status, daily dose.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  RefreshControl,
  Alert,
} from 'react-native';
import { useFocusEffect } from '@react-navigation/native';
import { WebSocket } from 'react-native';

// ── Types ──────────────────────────────────────────────────────────────

interface RoomData {
  id: string;
  name: string;
  spl: number;
  splUnit: string;
  trend: 'up' | 'down' | 'stable';
  maskingActive: boolean;
  maskingMode: string;
  occupancy: boolean;
}

interface SoundEvent {
  id: string;
  className: string;
  confidence: number;
  splDba: number;
  direction: number;
  timestamp: string;
  roomId: number;
}

interface DoseData {
  dailyDosePct: number;
  twaDba: number;
  peakDba: number;
  exposureMin: number;
}

// ── API Service ────────────────────────────────────────────────────────

const API_BASE = 'http://soundnest-hub.local:8000/api/v1';
const WS_BASE = 'ws://soundnest-hub.local:8000/ws';

async function fetchAPI(endpoint: string) {
  const response = await fetch(`${API_BASE}${endpoint}`);
  if (!response.ok) throw new Error(`API error: ${response.status}`);
  return response.json();
}

// ── Components ──────────────────────────────────────────────────────────

/** SPL gauge showing dB(A) with color-coded level */
const SPLGauge: React.FC<{ spl: number; roomName: string; maskingActive: boolean; maskingMode: string; occupancy: boolean }> = 
  ({ spl, roomName, maskingActive, maskingMode, occupancy }) => {
    const getColor = (db: number): string => {
      if (db < 40) return '#4CAF50';   // Green - quiet
      if (db < 55) return '#8BC34A';   // Light green
      if (db < 70) return '#FFEB3B';   // Yellow - moderate
      if (db < 85) return '#FF9800';   // Orange - loud
      if (db < 100) return '#F44336';  // Red - very loud
      return '#9C27B0';                // Purple - dangerous
    };

    const getLabel = (db: number): string => {
      if (db < 30) return 'Very Quiet';
      if (db < 40) return 'Quiet';
      if (db < 55) return 'Moderate';
      if (db < 70) return 'Fair';
      if (db < 85) return 'Loud';
      if (db < 100) return 'Very Loud';
      return 'Dangerous';
    };

    const color = getColor(spl);

    return (
      <View style={[styles.splCard, { borderLeftColor: color }]}>
        <View style={styles.splHeader}>
          <Text style={styles.roomName}>{roomName}</Text>
          <View style={[styles.occupancyDot, { backgroundColor: occupancy ? '#4CAF50' : '#9E9E9E' }]} />
        </View>
        <View style={styles.splValue}>
          <Text style={[styles.splNumber, { color }]}>{Math.round(spl)}</Text>
          <Text style={styles.splUnit}>dB(A)</Text>
        </View>
        <Text style={[styles.splLabel, { color }]}>{getLabel(spl)}</Text>
        {maskingActive && (
          <View style={styles.maskingBadge}>
            <Text style={styles.maskingBadgeText}>🎵 {maskingMode}</Text>
          </View>
        )}
      </View>
    );
  };

/** Sound dose circular progress */
const DoseGauge: React.FC<{ dosePct: number; twaDba: number; peakDba: number }> = 
  ({ dosePct, twaDba, peakDba }) => {
    const getColor = (pct: number): string => {
      if (pct < 50) return '#4CAF50';
      if (pct < 100) return '#FF9800';
      return '#F44336';
    };

    const color = getColor(dosePct);
    const circumference = 2 * Math.PI * 50;
    const progress = (dosePct / 100) * circumference;

    return (
      <View style={styles.doseCard}>
        <Text style={styles.doseTitle}>Sound Dose Today</Text>
        <View style={styles.doseCircle}>
          <Text style={[styles.dosePercent, { color }]}>
            {Math.round(dosePct)}%
          </Text>
          <Text style={styles.doseLabel}>of daily limit</Text>
        </View>
        <View style={styles.doseStats}>
          <View style={styles.doseStat}>
            <Text style={styles.doseStatValue}>{twaDba.toFixed(1)}</Text>
            <Text style={styles.doseStatLabel}>TWA dB(A)</Text>
          </View>
          <View style={styles.doseStat}>
            <Text style={styles.doseStatValue}>{peakDba.toFixed(1)}</Text>
            <Text style={styles.doseStatLabel}>Peak dB(A)</Text>
          </View>
        </View>
      </View>
    );
  };

/** Recent sound event card */
const EventCard: React.FC<{ event: SoundEvent }> = ({ event }) => {
  const getIcon = (cls: string): string => {
    const icons: Record<string, string> = {
      'Smoke Alarm': '🔥', 'CO Alarm': '☠️', 'Burglar Alarm': '🚨',
      'Doorbell': '🔔', 'Door Knock': '🚪', 'Crying Baby': '👶',
      'Dog Bark': '🐕', 'Cat Meow': '🐱', 'Speech': '🗣️',
      'Siren': '🚑', 'Car Horn': '📯', 'Glass Break': '💥',
      'Rain': '🌧️', 'Thunder': '⛈️', 'Wind': '💨',
      'TV': '📺', 'Music': '🎵', 'Vacuum': '🧹',
    };
    return icons[cls] || '🔊';
  };

  const getPriorityColor = (cls: string): string => {
    const critical = ['Smoke Alarm', 'CO Alarm', 'Glass Break'];
    const high = ['Doorbell', 'Crying Baby', 'Siren'];
    if (critical.includes(cls)) return '#F44336';
    if (high.includes(cls)) return '#FF9800';
    return '#FFEB3B';
  };

  const timeAgo = (ts: string): string => {
    const diff = Date.now() - new Date(ts).getTime();
    const mins = Math.floor(diff / 60000);
    if (mins < 1) return 'Just now';
    if (mins < 60) return `${mins}m ago`;
    const hrs = Math.floor(mins / 60);
    return `${hrs}h ago`;
  };

  return (
    <View style={[styles.eventCard, { borderLeftColor: getPriorityColor(event.className) }]}>
      <Text style={styles.eventIcon}>{getIcon(event.className)}</Text>
      <View style={styles.eventContent}>
        <Text style={styles.eventClass}>{event.className}</Text>
        <Text style={styles.eventMeta}>
          {Math.round(event.splDba)} dB(A) · {event.confidence}% · {timeAgo(event.timestamp)}
        </Text>
      </View>
    </View>
  );
};

// ── Main Dashboard Screen ──────────────────────────────────────────────

export const DashboardScreen: React.FC = () => {
  const [rooms, setRooms] = useState<RoomData[]>([]);
  const [events, setEvents] = useState<SoundEvent[]>([]);
  const [dose, setDose] = useState<DoseData>({ dailyDosePct: 0, twaDba: 0, peakDba: 0, exposureMin: 0 });
  const [refreshing, setRefreshing] = useState(false);
  const [connected, setConnected] = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // WebSocket connection for real-time updates
  useEffect(() => {
    const ws = new WebSocket(`${WS_BASE}/events`);
    
    ws.onopen = () => {
      setConnected(true);
      console.log('WebSocket connected');
    };
    
    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'event') {
          setEvents(prev => [data.data, ...prev].slice(0, 20));
        }
      } catch (e) {
        console.error('WebSocket parse error:', e);
      }
    };
    
    ws.onclose = () => {
      setConnected(false);
      console.log('WebSocket disconnected');
    };
    
    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };
    
    return () => {
      ws.close();
    };
  }, []);

  // Fetch data on screen focus
  useFocusEffect(
    useCallback(() => {
      fetchData();
      const interval = setInterval(fetchData, 5000);
      return () => clearInterval(interval);
    }, [])
  );

  const fetchData = async () => {
    try {
      // Fetch SPL data
      const splData = await fetchAPI('/spl/live');
      const roomsData: RoomData[] = Object.entries(splData.rooms || {}).map(([id, data]: [string, any]) => ({
        id,
        name: id === '0002' ? 'Living Room' : id === '0003' ? 'Bedroom' : 'Office',
        spl: data.dba || 0,
        splUnit: 'dB(A)',
        trend: 'stable',
        maskingActive: false,
        maskingMode: 'Off',
        occupancy: false,
      }));
      setRooms(roomsData);

      // Fetch dose data
      const doseData = await fetchAPI('/dose/today');
      setDose({
        dailyDosePct: doseData.daily_dose_pct || 0,
        twaDba: doseData.twa_dba || 0,
        peakDba: doseData.peak_dba || 0,
        exposureMin: doseData.exposure_min || 0,
      });

      // Fetch recent events
      const eventsData = await fetchAPI('/events?limit=10');
      setEvents(eventsData.map((e: any) => ({
        id: e.id,
        className: e.sound_class_name || 'Unknown',
        confidence: e.confidence || 0,
        splDba: e.spl_dba || 0,
        direction: e.direction_deg || 0,
        timestamp: e.timestamp,
        roomId: e.room_id || 0,
      })));

      setLastUpdate(new Date());
    } catch (error) {
      console.error('Fetch error:', error);
    }
  };

  const onRefresh = async () => {
    setRefreshing(true);
    await fetchData();
    setRefreshing(false);
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />}
    >
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.headerTitle}>SoundNest</Text>
        <View style={styles.connectionBadge}>
          <View style={[styles.connectionDot, { backgroundColor: connected ? '#4CAF50' : '#F44336' }]} />
          <Text style={styles.connectionText}>{connected ? 'Connected' : 'Offline'}</Text>
        </View>
      </View>

      {/* SPL Cards */}
      <Text style={styles.sectionTitle}>Room Sound Levels</Text>
      <ScrollView horizontal showsHorizontalScrollIndicator={false} style={styles.splScroll}>
        {rooms.map(room => (
          <SPLGauge
            key={room.id}
            spl={room.spl}
            roomName={room.name}
            maskingActive={room.maskingActive}
            maskingMode={room.maskingMode}
            occupancy={room.occupancy}
          />
        ))}
        {rooms.length === 0 && (
          <Text style={styles.emptyText}>No rooms connected</Text>
        )}
      </ScrollView>

      {/* Dose Gauge */}
      <DoseGauge
        dosePct={dose.dailyDosePct}
        twaDba={dose.twaDba}
        peakDba={dose.peakDba}
      />

      {/* Recent Events */}
      <Text style={styles.sectionTitle}>Recent Sounds</Text>
      {events.slice(0, 5).map(event => (
        <EventCard key={event.id} event={event} />
      ))}
      {events.length === 0 && (
        <Text style={styles.emptyText}>No recent sounds detected</Text>
      )}

      {/* Last Update */}
      <Text style={styles.lastUpdate}>
        Last update: {lastUpdate.toLocaleTimeString()}
      </Text>
    </ScrollView>
  );
};

// ── Styles ──────────────────────────────────────────────────────────────

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0A0A0A',
    padding: 16,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 20,
  },
  headerTitle: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#FFFFFF',
  },
  connectionBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1A1A1A',
    paddingVertical: 6,
    paddingHorizontal: 12,
    borderRadius: 16,
  },
  connectionDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    marginRight: 6,
  },
  connectionText: {
    fontSize: 12,
    color: '#CCCCCC',
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#FFFFFF',
    marginTop: 16,
    marginBottom: 12,
  },
  splScroll: {
    marginBottom: 8,
  },
  splCard: {
    backgroundColor: '#1A1A1A',
    borderRadius: 12,
    padding: 16,
    marginRight: 12,
    borderLeftWidth: 4,
    width: 160,
  },
  splHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  roomName: {
    fontSize: 14,
    fontWeight: '600',
    color: '#FFFFFF',
  },
  occupancyDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  splValue: {
    flexDirection: 'row',
    alignItems: 'baseline',
    marginBottom: 4,
  },
  splNumber: {
    fontSize: 40,
    fontWeight: 'bold',
  },
  splUnit: {
    fontSize: 14,
    color: '#888888',
    marginLeft: 4,
  },
  splLabel: {
    fontSize: 12,
    fontWeight: '500',
  },
  maskingBadge: {
    backgroundColor: '#2A2A2A',
    borderRadius: 8,
    paddingVertical: 4,
    paddingHorizontal: 8,
    marginTop: 8,
    alignSelf: 'flex-start',
  },
  maskingBadgeText: {
    fontSize: 11,
    color: '#4CAF50',
  },
  doseCard: {
    backgroundColor: '#1A1A1A',
    borderRadius: 12,
    padding: 20,
    alignItems: 'center',
    marginBottom: 8,
  },
  doseTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#FFFFFF',
    marginBottom: 16,
  },
  doseCircle: {
    width: 120,
    height: 120,
    borderRadius: 60,
    borderWidth: 8,
    borderColor: '#333333',
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 16,
  },
  dosePercent: {
    fontSize: 32,
    fontWeight: 'bold',
  },
  doseLabel: {
    fontSize: 10,
    color: '#888888',
  },
  doseStats: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    width: '100%',
  },
  doseStat: {
    alignItems: 'center',
  },
  doseStatValue: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#FFFFFF',
  },
  doseStatLabel: {
    fontSize: 12,
    color: '#888888',
  },
  eventCard: {
    backgroundColor: '#1A1A1A',
    borderRadius: 8,
    padding: 12,
    marginBottom: 8,
    flexDirection: 'row',
    alignItems: 'center',
    borderLeftWidth: 3,
  },
  eventIcon: {
    fontSize: 24,
    marginRight: 12,
  },
  eventContent: {
    flex: 1,
  },
  eventClass: {
    fontSize: 14,
    fontWeight: '600',
    color: '#FFFFFF',
  },
  eventMeta: {
    fontSize: 12,
    color: '#888888',
    marginTop: 2,
  },
  emptyText: {
    fontSize: 14,
    color: '#666666',
    textAlign: 'center',
    marginVertical: 20,
  },
  lastUpdate: {
    fontSize: 11,
    color: '#555555',
    textAlign: 'center',
    marginTop: 16,
    marginBottom: 32,
  },
});