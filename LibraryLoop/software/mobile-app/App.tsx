import React from 'react';
import { SafeAreaView, Text, View } from 'react-native';

// UI-only prototype: no network client, auth, seat reservation, or production semantics.
export default function App() {
  return <SafeAreaView><View><Text>LibraryLoop · demo</Text><Text>Study zone: availability unknown</Text><Text>Refresh from a site API client (not implemented)</Text></View></SafeAreaView>;
}
