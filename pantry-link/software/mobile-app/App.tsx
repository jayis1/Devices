import { useState } from 'react';
import { SafeAreaView, Text, View, Pressable } from 'react-native';

const API = 'http://127.0.0.1:8000'; // local demo only; configure authenticated HTTPS for deployment
export default function App() {
  const [status, setStatus] = useState('Not connected');
  async function check() {
    try { const r = await fetch(`${API}/health`); setStatus(r.ok ? 'Service reachable' : 'Service unavailable'); }
    catch { setStatus('Offline — use site hub / paper procedure'); }
  }
  return <SafeAreaView style={{ flex: 1, padding: 24 }}>
    <Text accessibilityRole="header" style={{ fontSize: 28, fontWeight: '700' }}>PantryLink</Text>
    <Text style={{ fontSize: 20, marginVertical: 20 }}>Site status: {status}</Text>
    <Pressable accessibilityRole="button" onPress={check} style={{ padding: 16, backgroundColor: '#174c38' }}>
      <Text style={{ color: 'white', fontSize: 18 }}>Check connection</Text>
    </Pressable>
    <View style={{ marginTop: 28 }}><Text style={{ fontSize: 18 }}>Availability: demo scaffold; not a reservation</Text></View>
  </SafeAreaView>;
}
