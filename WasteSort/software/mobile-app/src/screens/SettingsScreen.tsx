import React from 'react';
import { Text, View } from 'react-native';

export default function SettingsScreen() {
  return (
    <View style={{ backgroundColor: '#111827', borderRadius: 16, padding: 16, gap: 8 }}>
      <Text style={{ color: 'white', fontSize: 20, fontWeight: '600' }}>System settings</Text>
      <Text style={{ color: '#cbd5e1' }}>Municipality profile, quiet hours, household size, and node firmware controls.</Text>
    </View>
  );
}
