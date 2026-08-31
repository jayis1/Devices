import React from 'react';
import { SafeAreaView, ScrollView, Text, View } from 'react-native';

const Card = ({ title, body }: { title: string; body: string }) => (
  <View style={{ backgroundColor: '#fff', borderRadius: 16, padding: 16, marginBottom: 12 }}>
    <Text style={{ fontSize: 18, fontWeight: '700', marginBottom: 8 }}>{title}</Text>
    <Text style={{ fontSize: 15, lineHeight: 22 }}>{body}</Text>
  </View>
);

export default function App() {
  return (
    <SafeAreaView style={{ flex: 1, backgroundColor: '#f7d9e3' }}>
      <ScrollView contentContainerStyle={{ padding: 16 }}>
        <Text style={{ fontSize: 28, fontWeight: '800', marginBottom: 16, color: '#5e2750' }}>
          PregnancySync
        </Text>
        <Card title="Today" body="Movement check: due at 14:00. Morning BP captured. Hydration target: 2.3 L." />
        <Card title="Risk Feed" body="Reduced movement: medium. Hypertensive trend: medium. Supine sleep burden: low." />
        <Card title="Partner View" body="Quiet-hours safe. Escalate only on repeat reduced movement or high BP + symptoms." />
        <Card title="Clinician Export" body="Tap to share 7-day trend packet with movement, BP, strip, and sleep summaries." />
      </ScrollView>
    </SafeAreaView>
  );
}
