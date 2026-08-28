import React from 'react';
import { Text, View } from 'react-native';

export function FindScreen(): JSX.Element {
  return (
    <View style={{ padding: 14, borderWidth: 1, borderRadius: 12 }}>
      <Text style={{ fontSize: 20, fontWeight: '600' }}>Find Item</Text>
      <Text>Keys: entryway</Text>
      <Text>Backpack: office</Text>
      <Text>Laptop sleeve: office desk</Text>
    </View>
  );
}
