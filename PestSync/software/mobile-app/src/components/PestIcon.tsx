/**
 * PestIcon — Pest species icon (emoji-based for simplicity)
 */
import React from 'react';
import { Text, View, StyleSheet } from 'react-native';

const PEST_EMOJI: Record<number, string> = {
  0: '🐭', 1: '🐀', 2: '🪳', 3: '🪳',
  4: '🐜', 5: '🐜', 6: '🦟', 7: '🪰',
  8: '🦟', 9: '🛏️', 10: '🐜', 11: '🦗',
  12: '🕷️', 13: '🐛', 14: '🐞',
  255: '✅',
};

const PEST_COLORS: Record<number, string> = {
  0: '#e74c3c', 1: '#c0392b', 2: '#d35400', 3: '#e67e22',
  4: '#2ecc71', 5: '#27ae60', 6: '#3498db', 7: '#f1c40f',
  8: '#f1c40f', 9: '#8e44ad', 10: '#7f8c8d', 11: '#e74c3c',
  12: '#2c3e50', 13: '#bdc3c7', 14: '#95a5a6',
  255: '#2ecc71',
};

interface Props {
  pestClass: number | undefined;
  size?: number;
}

export default function PestIcon({ pestClass = 255, size = 40 }: Props) {
  const emoji = PEST_EMOJI[pestClass] || '❓';
  const color = PEST_COLORS[pestClass] || '#7f8c8d';

  return (
    <View style={[styles.icon, { backgroundColor: color + '30', width: size + 10, height: size + 10, borderRadius: (size + 10) / 2 }]}>
      <Text style={{ fontSize: size / 1.5 }}>{emoji}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  icon: { justifyContent: 'center', alignItems: 'center' },
});