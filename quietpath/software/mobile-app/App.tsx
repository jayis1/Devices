import { useEffect, useState } from 'react';
import { Text, View } from 'react-native';

type Reading = { node_id: number; sequence: number; leq_cdb: number; epoch_minutes: number };
const API = 'http://127.0.0.1:8000'; // configure a trusted local gateway URL in a real app
export default function App() {
  const [items, setItems] = useState<Reading[]>([]);
  useEffect(() => { fetch(`${API}/telemetry`).then(r => r.json()).then(setItems).catch(() => setItems([])); }, []);
  return <View style={{ padding: 24 }}><Text>QuietPath · indicative route context</Text>{items.map(x => <Text key={x.node_id}>Segment {x.node_id}: {x.leq_cdb === 65535 ? 'unknown' : `${(x.leq_cdb / 100).toFixed(1)} dB estimate`} · minute {x.epoch_minutes}</Text>)}{items.length === 0 && <Text>No current readings; treat route status as unknown.</Text>}</View>;
}
