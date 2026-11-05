import React from 'react';
import {SafeAreaView, Text, FlatList} from 'react-native';
const demo = [{id:'A12B', category:'electronics', status:'waiting'}];
export default function App() { return <SafeAreaView><Text>RepairLoop — operator queue (demo)</Text><FlatList data={demo} keyExtractor={x=>x.id} renderItem={({item})=><Text>{item.id} · {item.category} · {item.status}</Text>} /></SafeAreaView>; }
