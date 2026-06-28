/**
 * PestIDScreen — Photo → pest classification (on-device MobileNetV3)
 */
import React, { useState } from 'react';
import { View, Text, StyleSheet, TouchableOpacity, Image, Alert } from 'react-native';
import { launchCamera } from 'react-native-image-picker';
import PestIcon from '../components/PestIcon';

export default function PestIDScreen() {
  const [photo, setPhoto] = useState<string | null>(null);
  const [result, setResult] = useState<any>(null);
  const [analyzing, setAnalyzing] = useState(false);

  const takePhoto = () => {
    launchCamera({ mediaType: 'photo', cameraType: 'back' }, (response) => {
      if (response.assets?.[0]?.uri) {
        setPhoto(response.assets[0].uri);
        setResult(null);
        classify(response.assets[0].uri);
      }
    });
  };

  const classify = async (uri: string) => {
    setAnalyzing(true);
    // In production: run ONNX model inference on-device
    // For now, simulate result
    setTimeout(() => {
      const mockResult = {
        species: 'German Cockroach',
        confidence: 0.78,
        isPest: true,
        advice: 'Deploy gel bait stations in kitchen. Reduce moisture.',
      };
      setResult(mockResult);
      setAnalyzing(false);
    }, 2000);
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Pest ID</Text>
      <Text style={styles.subtitle}>Photograph any pest for instant identification</Text>

      {photo ? (
        <Image source={{ uri: photo }} style={styles.photo} resizeMode="contain" />
      ) : (
        <View style={styles.photoPlaceholder}>
          <Text style={styles.placeholderText}>📸 Take a photo of a pest</Text>
        </View>
      )}

      {analyzing && (
        <Text style={styles.analyzing}>Analyzing... (on-device AI)</Text>
      )}

      {result && (
        <View style={styles.resultCard}>
          <Text style={styles.resultSpecies}>{result.species}</Text>
          <Text style={styles.resultConfidence}>{Math.round(result.confidence * 100)}% confidence</Text>
          {result.isPest ? (
            <View>
              <Text style={styles.pestBadge}>⚠️ PEST</Text>
              <Text style={styles.advice}>{result.advice}</Text>
            </View>
          ) : (
            <Text style={styles.notPestBadge}>✅ Not a pest</Text>
          )}
        </View>
      )}

      <TouchableOpacity style={styles.button} onPress={takePhoto}>
        <Text style={styles.buttonText}>Take Photo</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#1a1a2e', padding: 20 },
  title: { color: '#fff', fontSize: 24, fontWeight: 'bold' },
  subtitle: { color: '#95a5a6', fontSize: 14, marginBottom: 20 },
  photo: { width: '100%', height: 300, borderRadius: 12, marginBottom: 15 },
  photoPlaceholder: { height: 300, backgroundColor: '#16213e', borderRadius: 12, justifyContent: 'center', alignItems: 'center', marginBottom: 15 },
  placeholderText: { color: '#7f8c8d', fontSize: 16 },
  analyzing: { color: '#3498db', fontSize: 16, textAlign: 'center', marginVertical: 10 },
  resultCard: { backgroundColor: '#16213e', padding: 15, borderRadius: 12, marginBottom: 15 },
  resultSpecies: { color: '#fff', fontSize: 20, fontWeight: 'bold' },
  resultConfidence: { color: '#95a5a6', fontSize: 14, marginTop: 4 },
  pestBadge: { color: '#e74c3c', fontSize: 16, fontWeight: 'bold', marginTop: 10 },
  notPestBadge: { color: '#2ecc71', fontSize: 16, fontWeight: 'bold', marginTop: 10 },
  advice: { color: '#bdc3c7', fontSize: 13, marginTop: 8, lineHeight: 18 },
  button: { backgroundColor: '#e74c3c', padding: 15, borderRadius: 12, alignItems: 'center' },
  buttonText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
});