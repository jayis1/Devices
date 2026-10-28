import React, {useEffect,useState} from 'react';
import {SafeAreaView,Text,FlatList} from 'react-native';
type Offer={id:string;category:string;status:string};
export default function App(){const [offers,setOffers]=useState<Offer[]>([]);useEffect(()=>{fetch('http://hub.local:8000/offers').then(r=>r.json()).then(setOffers).catch(()=>setOffers([]));},[]);return <SafeAreaView><Text>ReuseRelay — staff review required</Text><FlatList data={offers} keyExtractor={x=>x.id} renderItem={({item})=><Text>{item.category} · {item.status}</Text>}/></SafeAreaView>;}
