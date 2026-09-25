import React, {useEffect, useState} from 'react';
import {SafeAreaView, Text, Button} from 'react-native';
const API = process.env.EXPO_PUBLIC_TACTILESYNC_API ?? 'http://127.0.0.1:8000';
export default function App() { const [status,setStatus]=useState('Checking local hub…');
  useEffect(()=>{fetch(`${API}/health`).then(r=>r.json()).then(x=>setStatus(x.status)).catch(()=>setStatus('Hub unavailable'));},[]);
  return <SafeAreaView style={{padding:24,gap:16}}><Text accessibilityRole="header">TactileSync</Text><Text>{status}</Text><Button title="Refresh status" onPress={()=>setStatus('Refresh requested')}/><Text>Configure pattern meanings with the intended wearer at the local hub.</Text></SafeAreaView>; }
