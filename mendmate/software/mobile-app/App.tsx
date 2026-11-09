import React from 'react';
import {SafeAreaView, Text, View, Button} from 'react-native';
export default function App() {
  return <SafeAreaView><View style={{padding:24}}>
    <Text style={{fontSize:26,fontWeight:'700'}}>MendMate</Text>
    <Text>Repair notebook · local prototype UI</Text>
    <Text>Swatch reader: not connected</Text><Text>Thread tray: not connected</Text>
    <Button title="New repair note (demo)" onPress={() => {}} />
    <Text>Advice is informational; inspect fabric and choose a safe repair yourself.</Text>
  </View></SafeAreaView>;
}
