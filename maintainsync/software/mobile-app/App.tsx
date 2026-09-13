import React, {useEffect,useState} from 'react';
import {SafeAreaView,Text,FlatList,View} from 'react-native';
const API='http://localhost:8000';
export default function App(){const [cards,setCards]=useState<any[]>([]);useEffect(()=>{fetch(API+'/v1/cards',{headers:{Authorization:'Bearer change-me'}}).then(r=>r.json()).then(setCards).catch(()=>setCards([]))},[]);return <SafeAreaView><Text style={{fontSize:24}}>MaintainSync</Text><Text>Review evidence before service or any interlock action.</Text><FlatList data={cards} keyExtractor={(_,i)=>String(i)} renderItem={({item})=><View><Text>{item.asset_id}: {item.status}</Text></View>}/></SafeAreaView>}
