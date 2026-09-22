// Authored by jayis1. Prototype only; not a medical or safety controller.
import React, {useEffect, useState} from 'react';
import {SafeAreaView, Text, View} from 'react-native';
const API = 'http://localhost:8000';
export default function App() { const [status,setStatus]=useState('Checking local hub…'); useEffect(()=>{fetch(`${API}/health`).then(r=>r.json()).then(x=>setStatus(x.status==='ok'?'Hub available':'Hub unavailable')).catch(()=>setStatus('Hub unavailable or offline'));},[]); return <SafeAreaView><View><Text>SensorySync</Text><Text>{status}</Text><Text>Use physical controls for immediate comfort changes.</Text></View></SafeAreaView>; }
