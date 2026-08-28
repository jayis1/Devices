import React from 'react';
import { Text, View } from 'react-native';

export function RoutineScreen(): JSX.Element {
  return (
    <View style={{ padding: 14, borderWidth: 1, borderRadius: 12 }}>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Routines</Text>
      <Text>• Workday preflight</Text>
      <Text>• School pickup transition</Text>
      <Text>• Bedtime reset</Text>
    </View>
  );
}
