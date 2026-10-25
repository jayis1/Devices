import React from 'react';
import {SafeAreaView, Text, View} from 'react-native';

export default function App() {
  return <SafeAreaView><View style={{padding:24}}>
    <Text style={{fontSize:24,fontWeight:'bold'}}>RefillReady · Staff</Text>
    <Text>Site status: local prototype</Text>
    <Text>Open alerts: inspect fixture reports in the API</Text>
    <Text>Path checks: staff-entered only; not an accessibility certification</Text>
    <Text>Offline: last sync state must be treated as stale</Text>
  </View></SafeAreaView>;
}
