import React, {useEffect,useState} from 'react';
import {SafeAreaView,Text} from 'react-native';
export default function App(){const [rows,setRows]=useState<any[]>([]); useEffect(()=>{fetch('http://192.168.1.2:8000/v1/machines').then(r=>r.json()).then(setRows).catch(()=>setRows([{state:'offline'}]));},[]); return <SafeAreaView><Text>SpinShare — shared laundry status</Text>{rows.map((r,i)=><Text key={i}>Machine {r.node_id ?? '—'}: {r.state}</Text>)}</SafeAreaView>;}
