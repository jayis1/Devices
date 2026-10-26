import React from 'react';
import { SafeAreaView, Text, Button } from 'react-native';

export default function App() {
  const [message, setMessage] = React.useState('Outlet status unavailable until gateway connection is configured.');
  return <SafeAreaView style={{ padding: 24 }}>
    <Text>CivicCharge — staff preview</Text>
    <Text>{message}</Text>
    <Button title="Mark inspection needed" onPress={() => setMessage('Inspection request queued locally (demo only).')} />
  </SafeAreaView>;
}
