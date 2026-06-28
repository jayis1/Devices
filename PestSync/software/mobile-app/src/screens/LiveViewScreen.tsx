/**
 * LiveViewScreen — Real-time sentinel camera feed
 */
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, TouchableOpacity, Image } from 'react-native';
import { connectBLE, subscribeToCharacteristic } from '../ble/bleClient';

export default function LiveViewScreen() {
  const [connected, setConnected] = useState(false);
  const [frameData, setFrameData] = useState<string | null>(null);
  const [sentinels, setSentinels] = useState([
    { id: '0x0010', name: 'Kitchen Sentinel', battery: 85 },
    { id: '0x0011', name: 'Garage Sentinel', battery: 72 },
  ]);
  const [selected, setSelected] = useState(sentinels[0]);

  const handleConnect = async () => {
    try {
      await connectBLE(selected.id);
      setConnected(true);
      subscribeToCharacteristic('detection', (data: any) => {
        if (data.frame) setFrameData(`data:image/jpeg;base64,${data.frame}`);
      });
    } catch (e) {
      console.warn('BLE connect failed', e);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Live View</Text>

      {/* Sentinel selector */}
      <View style={styles.selector}>
        {sentinels.map((s) => (
          <TouchableOpacity
            key={s.id}
            style={[styles.sentinelBtn, selected.id === s.id && styles.selectedBtn]}
            onPress={() => setSelected(s)}
          >
            <Text style={styles.sentinelName}>{s.name}</Text>
            <Text style={styles.sentinelBat}>🔋 {s.battery}%</Text>
          </TouchableOpacity>
        ))}
      </View>

      {/* Camera feed */}
      <View style={styles.cameraView}>
        {frameData ? (
          <Image source={{ uri: frameData }} style={styles.frame} resizeMode="contain" />
        ) : (
          <View style={styles.placeholder}>
            <Text style={styles.placeholderText}>
              {connected ? 'Waiting for frame...' : 'Connect to view live feed'}
            </Text>
          </View>
        )}
      </View>

      {/* Connect button */}
      {!connected && (
        <TouchableOpacity style={styles.connectBtn} onPress={handleConnect}>
          <Text style={styles.connectBtnText}>Connect via BLE</Text>
        </TouchableOpacity>
      )}

      {/* Detection info overlay */}
      {connected && (
        <View style={styles.overlay}>
          <Text style={styles.overlayText}>● LIVE · {selected.name}</Text>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e', padding: 15 },
  title: { color: '#fff', fontSize: 22, fontWeight: 'bold', marginBottom: 15 },
  selector: { flexDirection: 'row', marginBottom: 15 },
  sentinelBtn: { backgroundColor: '#16213e', padding: 10, borderRadius: 8, marginRight: 10 },
  selectedBtn: { backgroundColor: '#e74c3c' },
  sentinelName: { color: '#fff', fontSize: 13, fontWeight: 'bold' },
  sentinelBat: { color: '#95a5a6', fontSize: 11, marginTop: 2 },
  cameraView: { flex: 1, backgroundColor: '#000', borderRadius: 12, overflow: 'hidden' },
  frame: { flex: 1, width: '100%' },
  placeholder: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  placeholderText: { color: '#7f8c8d', fontSize: 16 },
  connectBtn: { backgroundColor: '#e74c3c', padding: 15, borderRadius: 12, marginTop: 15, alignItems: 'center' },
  connectBtnText: { color: '#fff', fontSize: 16, fontWeight: 'bold' },
  overlay: { position: 'absolute', top: 80, left: 25, backgroundColor: 'rgba(0,0,0,0.6)', padding: 5, borderRadius: 4 },
  overlayText: { color: '#e74c3c', fontSize: 12, fontWeight: 'bold' },
});