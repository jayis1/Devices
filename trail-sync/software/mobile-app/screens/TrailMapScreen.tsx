/**
 * TrailSync — Live Trail Map Screen
 *
 * Shows runner position on offline trail map with breadcrumbs,
 * nearest beacons, water sources, and off-trail alerts.
 * SPDX-License-Identifier: MIT
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, Dimensions } from 'react-native';
import { api } from '../api';

const { width, height } = Dimensions.get('window');

interface Props { runnerId: string; }

export function TrailMapScreen({ runnerId }: Props) {
  const [position, setPosition] = useState({ lat: 0, lon: 0, alt: 0, speed: 0, sats: 0 });
  const [beacons, setBeacons] = useState<any[]>([]);
  const [offTrail, setOffTrail] = useState(false);

  useEffect(() => {
    const fetch = async () => {
      try {
        const runner = await api.getRunner(runnerId);
        if (runner) {
          setPosition({
            lat: runner.lat, lon: runner.lon,
            alt: runner.altitude_m, speed: runner.speed_cm_s,
            sats: runner.num_satellites,
          });
        }
        const b = await api.listBeacons();
        setBeacons(Object.values(b));
      } catch (e) { /* offline */ }
    };
    fetch();
    const interval = setInterval(fetch, 5000);
    return () => clearInterval(interval);
  }, [runnerId]);

  return (
    <View style={styles.container}>
      <View style={styles.mapPlaceholder}>
        <Text style={styles.mapTitle}>🗺️ Trail Map</Text>
        <Text style={styles.coords}>
          {position.lat.toFixed(5)}, {position.lon.toFixed(5)}
        </Text>
        <Text style={styles.alt}>Alt: {position.alt}m  Sats: {position.sats}</Text>
        <Text style={styles.speed}>
          Speed: {(position.speed / 100).toFixed(1)} km/h
        </Text>
        {offTrail && (
          <View style={styles.offTrailAlert}>
            <Text style={styles.offTrailText}>⚠ OFF TRAIL — Return to marked path</Text>
          </View>
        )}
      </View>

      <View style={styles.beaconList}>
        <Text style={styles.sectionTitle}>Nearby Beacons</Text>
        {beacons.slice(0, 5).map((b: any, i: number) => (
          <View key={i} style={styles.beaconItem}>
            <Text style={styles.beaconName}>Beacon {b.beacon_id}</Text>
            <Text style={styles.beaconInfo}>
              Difficulty: {b.trail_difficulty}  Water: {b.water_available ? '✓' : '✗'}
            </Text>
            <Text style={styles.beaconInfo}>
              Temp: {(b.temp_c / 100).toFixed(1)}°C  Hazards: {b.hazard_flags || 'none'}
            </Text>
          </View>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117' },
  mapPlaceholder: {
    flex: 2, backgroundColor: '#161b22', justifyContent: 'center', alignItems: 'center',
    borderWidth: 1, borderColor: '#30363d', margin: 8, borderRadius: 12,
  },
  mapTitle: { fontSize: 24, color: '#4cc9f0', fontWeight: 'bold' },
  coords: { fontSize: 16, color: '#e6edf3', marginTop: 8 },
  alt: { fontSize: 14, color: '#8b949e', marginTop: 4 },
  speed: { fontSize: 14, color: '#8b949e', marginTop: 4 },
  offTrailAlert: {
    backgroundColor: '#f85149', padding: 12, borderRadius: 8, marginTop: 12,
  },
  offTrailText: { color: 'white', fontSize: 16, fontWeight: 'bold', textAlign: 'center' },
  beaconList: { flex: 1, padding: 8 },
  sectionTitle: { fontSize: 16, color: '#4cc9f0', fontWeight: 'bold', marginBottom: 8 },
  beaconItem: {
    backgroundColor: '#161b22', padding: 10, borderRadius: 8,
    marginBottom: 6, borderWidth: 1, borderColor: '#30363d',
  },
  beaconName: { fontSize: 14, color: '#e6edf3', fontWeight: 'bold' },
  beaconInfo: { fontSize: 12, color: '#8b949e', marginTop: 2 },
});