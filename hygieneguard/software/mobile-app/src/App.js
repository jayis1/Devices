// HygieneGuard offline-aware React Native scaffold. Author: jayis1.
import React, {useEffect, useState} from 'react';
import {SafeAreaView, Text} from 'react-native';
export default function App() {
  const [status, setStatus] = useState('offline / unknown');
  useEffect(() => { fetch('http://localhost:8000/health').then(r => r.ok ? setStatus('hub reachable') : null).catch(() => setStatus('offline / unknown')); }, []);
  return <SafeAreaView><Text>HygieneGuard: {status}</Text><Text>Guidance only; use ordinary handwashing whenever needed.</Text></SafeAreaView>;
}
