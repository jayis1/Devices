import React from 'react';
import { Text, View } from 'react-native';

export default function ProfilesScreen() {
  return (
    <View>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Allergy Profiles</Text>
      <Text>Manages allergen lists, severity, school-safe foods, and caregiver sharing.</Text>
    </View>
  );
}
