// QueueKind mobile scaffold — not built. Displays only aggregate, advisory queue estimates.
import React from 'react';
import { SafeAreaView, Text } from 'react-native';
export default function App() {
  return <SafeAreaView accessibilityLabel="QueueKind wait estimate">
    <Text accessibilityRole="header">Wait estimate</Text>
    <Text>Connect to your service site for an advisory estimate.</Text>
  </SafeAreaView>;
}
