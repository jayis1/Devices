import React from 'react';
import { Text, View } from 'react-native';
import { useSchoolSync } from '../services/SchoolSyncContext';

export default function DashboardScreen() {
  const { children } = useSchoolSync();
  return (
    <View style={{ margin: 16 }}>
      {children.map((child) => (
        <Text key={child.id}>{child.id}: {child.readiness} - {child.status}</Text>
      ))}
    </View>
  );
}
