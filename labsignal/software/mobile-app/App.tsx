import React from 'react';
import {SafeAreaView, Text, Button, View} from 'react-native';
export default function App() {
  const [status, setStatus] = React.useState('Not connected — inspect local gateway');
  return <SafeAreaView style={{padding:24, gap:16}}>
    <Text accessibilityRole="header" style={{fontSize:26, fontWeight:'700'}}>LabSignal</Text>
    <Text accessibilityLiveRegion="polite">Instrument status: {status}</Text>
    <View><Button title="Refresh prototype" onPress={() => setStatus('Demo only; API hookup not configured')} /></View>
    <Text>Calibration records require operator confirmation. Not a measurement or safety instrument.</Text>
  </SafeAreaView>;
}
