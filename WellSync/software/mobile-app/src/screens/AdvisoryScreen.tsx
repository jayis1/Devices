import React from 'react';
import { Text, View } from 'react-native';

export default function AdvisoryScreen() {
  return (
    <View style={{ backgroundColor: '#1f2937', padding: 16, borderRadius: 16 }}>
      <Text style={{ color: 'white', fontSize: 18, fontWeight: '600' }}>Advisory workflow</Text>
      <Text style={{ color: '#cbd5e1', marginTop: 8 }}>Provides boil/do-not-drink guidance, sample collection checklist, and remediation confirmation.</Text>
    </View>
  );
}
