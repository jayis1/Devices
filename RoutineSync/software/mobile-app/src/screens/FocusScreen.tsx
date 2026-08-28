import React from 'react';
import { Text, View } from 'react-native';

export function FocusScreen(): JSX.Element {
  return (
    <View style={{ padding: 14, borderWidth: 1, borderRadius: 12 }}>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Focus</Text>
      <Text>Current mode: Deep work</Text>
      <Text>Suggested break in 8 minutes</Text>
      <Text>Beacon cue: warm light + single tone</Text>
    </View>
  );
}
