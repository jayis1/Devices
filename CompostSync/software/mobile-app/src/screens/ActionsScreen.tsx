import React from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity } from 'react-native';
import { useStore } from '../store/store';

export default function ActionsScreen() {
  const { actions, completeAction } = useStore();

  const renderItem = ({ item }: { item: any }) => (
    <TouchableOpacity
      style={[styles.actionCard, item.completed && styles.completedCard]}
      onPress={() => !item.completed && completeAction(item.id)}
    >
      <View style={styles.actionIcon}>
        <Text style={{ fontSize: 28 }}>{item.icon}</Text>
      </View>
      <View style={styles.actionInfo}>
        <Text style={styles.actionTitle}>{item.title}</Text>
        <Text style={styles.actionDesc}>{item.description}</Text>
        <Text style={styles.actionTime}>{item.time}</Text>
      </View>
      {item.completed ? (
        <Text style={styles.checkIcon}>✓</Text>
      ) : (
        <Text style={styles.arrowIcon}>›</Text>
      )}
    </TouchableOpacity>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Actions</Text>
      <FlatList
        data={actions}
        renderItem={renderItem}
        keyExtractor={(item) => item.id}
        contentContainerStyle={{ paddingBottom: 20 }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#121212', padding: 16 },
  title: { fontSize: 22, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  actionCard: {
    flexDirection: 'row', alignItems: 'center', padding: 16,
    backgroundColor: '#1E1E1E', borderRadius: 12, marginBottom: 8,
  },
  completedCard: { opacity: 0.5 },
  actionIcon: { width: 48, height: 48, borderRadius: 24, backgroundColor: '#2A2A2A', justifyContent: 'center', alignItems: 'center' },
  actionInfo: { flex: 1, marginLeft: 12 },
  actionTitle: { fontSize: 16, fontWeight: 'bold', color: '#fff' },
  actionDesc: { fontSize: 13, color: '#888', marginTop: 4 },
  actionTime: { fontSize: 12, color: '#666', marginTop: 4 },
  checkIcon: { fontSize: 24, color: '#4CAF50' },
  arrowIcon: { fontSize: 24, color: '#666' },
});