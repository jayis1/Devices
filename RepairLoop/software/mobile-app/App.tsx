import React from 'react';
import { SafeAreaView, Text } from 'react-native';

// UI scaffold only; production identity, authorization and secure storage are not implemented.
export default function App() {
  return <SafeAreaView><Text>RepairLoop — local bench readiness</Text><Text>Connect to an operator-configured local API.</Text></SafeAreaView>;
}
