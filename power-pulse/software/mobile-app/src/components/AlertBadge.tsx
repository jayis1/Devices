/**
 * PowerPulse — Alert Badge Component
 */

import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface AlertBadgeProps {
  count: number;
  severity: number;
}

export function AlertBadge({ count, severity }: AlertBadgeProps) {
  const color = severity >= 4 ? '#ff0000' : severity >= 3 ? '#ff4444' : severity >= 2 ? '#ff6600' : '#ffaa00';
  
  return (
    <View style={[styles.badge, { backgroundColor: color }]}>
      <Text style={styles.text}>{count} Alert{count !== 1 ? 's' : ''}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  badge: { borderRadius: 8, paddingVertical: 10, paddingHorizontal: 16, marginBottom: 12 },
  text: { color: '#fff', fontSize: 16, fontWeight: '700', textAlign: 'center' },
});