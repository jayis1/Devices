import React, { useState, useRef } from 'react';
import { View, Text, StyleSheet, TouchableOpacity, Image } from 'react-native';
import { RNCamera } from 'react-native-camera';

const COMPOSTABLE_ITEMS: Record<string, { compostable: boolean; category: string; cn: number }> = {
  'apple_core': { compostable: true, category: 'Green', cn: 20 },
  'banana_peel': { compostable: true, category: 'Green', cn: 25 },
  'coffee_grounds': { compostable: true, category: 'Green', cn: 15 },
  'eggshell': { compostable: true, category: 'Green', cn: 10 },
  'cardboard': { compostable: true, category: 'Brown', cn: 350 },
  'plastic_bottle': { compostable: false, category: 'Recycle', cn: 0 },
  'meat': { compostable: false, category: 'Not recommended', cn: 0 },
};

export default function ScannerScreen() {
  const [result, setResult] = useState<string | null>(null);
  const [scanning, setScanning] = useState(false);
  const cameraRef = useRef<RNCamera>(null);

  const takePicture = async () => {
    if (cameraRef.current) {
      try {
        const data = await cameraRef.current.takePictureAsync({ quality: 0.5 });
        setScanning(true);
        // In production: send to cloud API for MobileNetV3 classification
        // For now, simulate result
        setTimeout(() => {
          setResult('apple_core');
          setScanning(false);
        }, 1500);
      } catch (e) {
        console.error(e);
        setScanning(false);
      }
    }
  };

  const item = result ? COMPOSTABLE_ITEMS[result] : null;

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Can I compost this?</Text>

      <View style={styles.cameraContainer}>
        <RNCamera
          ref={cameraRef}
          style={styles.camera}
          type={RNCamera.Constants.Type.back}
          captureAudio={false}
        />
        {!scanning && !result && (
          <TouchableOpacity style={styles.captureButton} onPress={takePicture}>
            <Text style={styles.captureText}>📸 Scan</Text>
          </TouchableOpacity>
        )}
        {scanning && (
          <View style={styles.scanningOverlay}>
            <Text style={styles.scanningText}>Analyzing...</Text>
          </View>
        )}
      </View>

      {item && (
        <View style={[styles.resultCard, { borderLeftColor: item.compostable ? '#4CAF50' : '#F44336' }]}>
          <Text style={styles.resultIcon}>{item.compostable ? '✅' : '🚫'}</Text>
          <View style={styles.resultInfo}>
            <Text style={styles.resultVerdict}>
              {item.compostable ? 'Yes, compost it!' : 'No, don\'t compost this'}
            </Text>
            <Text style={styles.resultCategory}>{item.category}</Text>
            {item.compostable && (
              <Text style={styles.resultCN}>C:N ratio: {item.cn}:1</Text>
            )}
          </View>
        </View>
      )}

      {result && (
        <TouchableOpacity onPress={() => setResult(null)} style={styles.resetButton}>
          <Text style={styles.resetText}>Scan another</Text>
        </TouchableOpacity>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#121212', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', textAlign: 'center', marginVertical: 16 },
  cameraContainer: { flex: 1, borderRadius: 16, overflow: 'hidden', marginBottom: 16 },
  camera: { flex: 1 },
  captureButton: {
    position: 'absolute', bottom: 20, alignSelf: 'center',
    backgroundColor: '#4CAF50', paddingVertical: 12, paddingHorizontal: 32,
    borderRadius: 30,
  },
  captureText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  scanningOverlay: { position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, justifyContent: 'center', alignItems: 'center', backgroundColor: 'rgba(0,0,0,0.6)' },
  scanningText: { color: '#fff', fontSize: 20 },
  resultCard: { flexDirection: 'row', alignItems: 'center', padding: 16, backgroundColor: '#1E1E1E', borderRadius: 12, borderLeftWidth: 4, marginBottom: 12 },
  resultIcon: { fontSize: 36, marginRight: 16 },
  resultInfo: { flex: 1 },
  resultVerdict: { fontSize: 18, fontWeight: 'bold', color: '#fff' },
  resultCategory: { fontSize: 14, color: '#888', marginTop: 4 },
  resultCN: { fontSize: 14, color: '#4CAF50', marginTop: 4 },
  resetButton: { alignSelf: 'center', padding: 12 },
  resetText: { color: '#4CAF50', fontSize: 16 },
});