import React from 'react';
import { Text, View } from 'react-native';

export default function SortGuideScreen() {
  return (
    <View style={{ backgroundColor: '#111827', borderRadius: 16, padding: 16, gap: 8 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>Sort guide</Text>
      <Text style={{ color: '#cbd5e1' }}>Item-by-item guidance with municipality-specific rules and uncertain-item review flow.</Text>
    </View>
  );
}
