import React from 'react';
import { Text, View } from 'react-native';

export default function ScannerScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Package Scanner</Text>
      <Text>Shows barcode, OCR ingredients, allergen verdict, and approved profiles.</Text>
    </View>
  );
}
