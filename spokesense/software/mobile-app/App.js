import React from 'react';
import { SafeAreaView, Text, FlatList } from 'react-native';
const demo = [{id:'wheel-1',kind:'Wheel deviation',value:'—'}, {id:'stand-1',kind:'Pressure / lever fixture',value:'—'}];
export default function App(){return <SafeAreaView style={{padding:24}}><Text style={{fontSize:24}}>SpokeSense</Text><Text>Maintenance observations only — not a safety assessment.</Text><FlatList data={demo} keyExtractor={x=>x.id} renderItem={({item})=><Text style={{padding:12}}>{item.id}: {item.kind} · {item.value}</Text>}/></SafeAreaView>}
