import React from 'react';
import { Text, View } from 'react-native';

export default function ReportsScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Reports</Text>
      <Text>Clinician export: cycle length, heavy-flow days, pain response, strip history</Text>
      <Text>Privacy toggle: raw physiology upload disabled by default</Text>
    </View>
  );
}
