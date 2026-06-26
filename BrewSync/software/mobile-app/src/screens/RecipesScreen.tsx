import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function RecipesScreen() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>📖 Recipes</Text>
      <View style={styles.card}>
        <Text style={styles.placeholder}>[Recipe library, BeerXML import, style guidelines]</Text>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117', padding: 16 },
  title: { fontSize: 24, fontWeight: 'bold', color: '#fff', marginBottom: 16 },
  card: { backgroundColor: '#161b22', borderRadius: 12, padding: 16, marginBottom: 12 },
  placeholder: { color: '#8b949e', fontSize: 14 },
});