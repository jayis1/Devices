import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Switch,
  Slider,
} from 'react-native';

const API_BASE = 'http://soundnest-hub.local:8000/api/v1';

interface MaskingStatus {
  room_id: number;
  room_name: string;
  active: boolean;
  mode: string;
  mode_name: string;
  volume: number;
  stereo_balance: number;
  adaptive: boolean;
  duration_min: number;
  remaining_min: number;
  target_spl_dba: number;
  ambient_spl_dba: number;
}

const MASKING_MODES = [
  { key: 'off', label: 'Off', icon: '⏹' },
  { key: 'white', label: 'White Noise', icon: '📻' },
  { key: 'pink', label: 'Pink Noise', icon: '🎵' },
  { key: 'brown', label: 'Brown Noise', icon: '🌊' },
  { key: 'rain', label: 'Rain', icon: '🌧' },
  { key: 'stream', label: 'Stream', icon: '🏞' },
  { key: 'forest', label: 'Forest', icon: '🌲' },
  { key: 'ocean', label: 'Ocean', icon: '🏝' },
  { key: 'privacy', label: 'Privacy', icon: '🔒' },
  { key: 'tinnitus', label: 'Tinnitus', icon: '🔊' },
];

export default function MaskingScreen() {
  const [statuses, setStatuses] = useState<MaskingStatus[]>([]);
  const [selectedRoom, setSelectedRoom] = useState<number>(0);
  const [volume, setVolume] = useState(50);
  const [balance, setBalance] = useState(50);
  const [adaptive, setAdaptive] = useState(true);
  const [duration, setDuration] = useState(60);
  const [tinnitusFreq, setTinnitusFreq] = useState(6000);

  useEffect(() => {
    fetchMaskingStatus();
    const interval = setInterval(fetchMaskingStatus, 5000);
    return () => clearInterval(interval);
  }, []);

  const fetchMaskingStatus = async () => {
    try {
      const response = await fetch(`${API_BASE}/masking/status`);
      if (response.ok) {
        const data = await response.json();
        setStatuses(data.rooms || []);
        if (data.rooms && data.rooms.length > 0 && selectedRoom === 0) {
          setSelectedRoom(data.rooms[0].room_id);
        }
      }
    } catch (error) {
      console.error('Failed to fetch masking status:', error);
    }
  };

  const startMasking = useCallback(async (mode: string) => {
    try {
      await fetch(`${API_BASE}/masking/start`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          room_id: selectedRoom,
          mode,
          volume,
          stereo_balance: balance,
          fade_in_ms: 30,
          fade_out_ms: 50,
          duration_min: duration,
          adaptive,
        }),
      });
      fetchMaskingStatus();
    } catch (error) {
      console.error('Failed to start masking:', error);
    }
  }, [selectedRoom, volume, balance, duration, adaptive]);

  const stopMasking = useCallback(async () => {
    try {
      await fetch(`${API_BASE}/masking/stop?room_id=${selectedRoom}`, {
        method: 'POST',
      });
      fetchMaskingStatus();
    } catch (error) {
      console.error('Failed to stop masking:', error);
    }
  }, [selectedRoom]);

  const currentStatus = statuses.find(s => s.room_id === selectedRoom);

  return (
    <ScrollView style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.title}>Sound Masking</Text>
        <Text style={styles.subtitle}>
          {currentStatus?.active ? '● Active' : '○ Inactive'}
        </Text>
      </View>

      {/* Room Selector */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Room</Text>
        <ScrollView horizontal showsHorizontalScrollIndicator={false}>
          {statuses.map(room => (
            <TouchableOpacity
              key={room.room_id}
              style={[
                styles.roomChip,
                selectedRoom === room.room_id && styles.roomChipActive,
              ]}
              onPress={() => setSelectedRoom(room.room_id)}
            >
              <Text style={[
                styles.roomChipText,
                selectedRoom === room.room_id && styles.roomChipTextActive,
              ]}>
                {room.room_name}
              </Text>
              {room.active && <View style={styles.activeDot} />}
            </TouchableOpacity>
          ))}
        </ScrollView>
      </View>

      {/* Current Status */}
      {currentStatus?.active && (
        <View style={styles.statusCard}>
          <View style={styles.statusRow}>
            <Text style={styles.statusLabel}>Mode</Text>
            <Text style={styles.statusValue}>{currentStatus.mode_name}</Text>
          </View>
          <View style={styles.statusRow}>
            <Text style={styles.statusLabel}>Volume</Text>
            <Text style={styles.statusValue}>{currentStatus.volume}%</Text>
          </View>
          <View style={styles.statusRow}>
            <Text style={styles.statusLabel}>Ambient</Text>
            <Text style={styles.statusValue}>{currentStatus.ambient_spl_dba.toFixed(1)} dB(A)</Text>
          </View>
          <View style={styles.statusRow}>
            <Text style={styles.statusLabel}>Target</Text>
            <Text style={styles.statusValue}>{currentStatus.target_spl_dba.toFixed(1)} dB(A)</Text>
          </View>
          {currentStatus.remaining_min > 0 && (
            <View style={styles.statusRow}>
              <Text style={styles.statusLabel}>Remaining</Text>
              <Text style={styles.statusValue}>{currentStatus.remaining_min} min</Text>
            </View>
          )}
        </View>
      )}

      {/* Masking Modes */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Masking Sound</Text>
        <View style={styles.modeGrid}>
          {MASKING_MODES.map(mode => (
            <TouchableOpacity
              key={mode.key}
              style={[
                styles.modeButton,
                currentStatus?.mode === mode.key && styles.modeButtonActive,
              ]}
              onPress={() => {
                if (mode.key === 'off') {
                  stopMasking();
                } else {
                  startMasking(mode.key);
                }
              }}
            >
              <Text style={styles.modeIcon}>{mode.icon}</Text>
              <Text style={[
                styles.modeLabel,
                currentStatus?.mode === mode.key && styles.modeLabelActive,
              ]}>
                {mode.label}
              </Text>
            </TouchableOpacity>
          ))}
        </View>
      </View>

      {/* Volume Control */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Volume: {volume}%</Text>
        <Slider
          style={styles.slider}
          minimumValue={10}
          maximumValue={100}
          step={5}
          value={volume}
          onValueChange={setVolume}
          minimumTrackTintColor="#4ECDC4"
          maximumTrackTintColor="#555"
          thumbTintColor="#4ECDC4"
        />
      </View>

      {/* Stereo Balance */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Balance: L {100 - balance}% / R {balance}%</Text>
        <Slider
          style={styles.slider}
          minimumValue={0}
          maximumValue={100}
          step={5}
          value={balance}
          onValueChange={setBalance}
          minimumTrackTintColor="#4ECDC4"
          maximumTrackTintColor="#555"
          thumbTintColor="#4ECDC4"
        />
      </View>

      {/* Duration */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>
          Duration: {duration === 0 ? '∞' : `${duration} min`}
        </Text>
        <Slider
          style={styles.slider}
          minimumValue={0}
          maximumValue={480}
          step={15}
          value={duration}
          onValueChange={setDuration}
          minimumTrackTintColor="#4ECDC4"
          maximumTrackTintColor="#555"
          thumbTintColor="#4ECDC4"
        />
        <Text style={styles.sliderHint}>
          0 = continuous, slide to set auto-stop timer
        </Text>
      </View>

      {/* Adaptive Toggle */}
      <View style={styles.section}>
        <View style={styles.switchRow}>
          <View>
            <Text style={styles.switchLabel}>Adaptive Volume</Text>
            <Text style={styles.switchHint}>
              Adjusts volume based on ambient noise level
            </Text>
          </View>
          <Switch
            value={adaptive}
            onValueChange={setAdaptive}
            trackColor={{ false: '#555', true: '#4ECDC4' }}
            thumbColor="#fff"
          />
        </View>
      </View>

      {/* Tinnitus Settings */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Tinnitus Frequency: {tinnitusFreq} Hz</Text>
        <Slider
          style={styles.slider}
          minimumValue={500}
          maximumValue={15000}
          step={100}
          value={tinnitusFreq}
          onValueChange={setTinnitusFreq}
          minimumTrackTintColor="#FF6B6B"
          maximumTrackTintColor="#555"
          thumbTintColor="#FF6B6B"
        />
        <Text style={styles.sliderHint}>
          Select "Tinnitus" mode above, then adjust to match your tinnitus pitch
        </Text>
      </View>

      {/* Stop Button */}
      {currentStatus?.active && (
        <TouchableOpacity style={styles.stopButton} onPress={stopMasking}>
          <Text style={styles.stopButtonText}>Stop Masking</Text>
        </TouchableOpacity>
      )}

      <View style={styles.spacer} />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0D1B2A',
  },
  header: {
    padding: 20,
    paddingTop: 60,
    backgroundColor: '#1B2838',
  },
  title: {
    fontSize: 28,
    fontWeight: '700',
    color: '#FFFFFF',
  },
  subtitle: {
    fontSize: 14,
    color: '#8A9BA8',
    marginTop: 4,
  },
  section: {
    margin: 16,
    padding: 16,
    backgroundColor: '#1B2838',
    borderRadius: 12,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#4ECDC4',
    marginBottom: 8,
  },
  roomChip: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 20,
    backgroundColor: '#2A3A4A',
    marginRight: 8,
    flexDirection: 'row',
    alignItems: 'center',
  },
  roomChipActive: {
    backgroundColor: '#4ECDC4',
  },
  roomChipText: {
    fontSize: 14,
    color: '#E0E0E0',
    fontWeight: '500',
  },
  roomChipTextActive: {
    color: '#0D1B2A',
  },
  activeDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: '#FF6B6B',
    marginLeft: 6,
  },
  statusCard: {
    margin: 16,
    padding: 16,
    backgroundColor: '#1B3A38',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#4ECDC4',
  },
  statusRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 4,
  },
  statusLabel: {
    fontSize: 14,
    color: '#8A9BA8',
  },
  statusValue: {
    fontSize: 14,
    color: '#4ECDC4',
    fontWeight: '600',
  },
  modeGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
  },
  modeButton: {
    width: '30%',
    aspectRatio: 1.2,
    borderRadius: 12,
    backgroundColor: '#2A3A4A',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 8,
  },
  modeButtonActive: {
    backgroundColor: '#4ECDC4',
  },
  modeIcon: {
    fontSize: 24,
    marginBottom: 4,
  },
  modeLabel: {
    fontSize: 11,
    color: '#E0E0E0',
    fontWeight: '500',
  },
  modeLabelActive: {
    color: '#0D1B2A',
  },
  slider: {
    marginVertical: 4,
  },
  sliderHint: {
    fontSize: 12,
    color: '#8A9BA8',
    marginTop: 4,
  },
  switchRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  switchLabel: {
    fontSize: 16,
    color: '#E0E0E0',
    fontWeight: '500',
  },
  switchHint: {
    fontSize: 12,
    color: '#8A9BA8',
    marginTop: 2,
  },
  stopButton: {
    margin: 16,
    backgroundColor: '#FF6B6B',
    borderRadius: 12,
    padding: 16,
    alignItems: 'center',
  },
  stopButtonText: {
    fontSize: 18,
    fontWeight: '700',
    color: '#FFFFFF',
  },
  spacer: {
    height: 40,
  },
});