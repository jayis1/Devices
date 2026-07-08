import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet, RefreshControl } from 'react-native';
import { Card, Title, Paragraph, Badge, Button } from 'react-native-paper';
import { client } from '../api/client';

export default function LiveDashboardScreen() {
  const [nodes, setNodes] = useState<any[]>([]);
  const [latestEvent, setLatestEvent] = useState<any | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const refresh = async () => {
    setRefreshing(true);
    try {
      const [nodeList, events] = await Promise.all([
        client.getNodes(),
        client.getEvents(1),
      ]);
      setNodes(nodeList);
      setLatestEvent(events[0] || null);
    } catch (e) {
      console.error(e);
    }
    setRefreshing(false);
  };

  useEffect(() => {
    client.init().then(refresh);
    const interval = setInterval(refresh, 30000);
    return () => clearInterval(interval);
  }, []);

  const onlineNodes = nodes.filter((n: any) => n.status === 'online');
  const floorNodes = nodes.filter((n: any) => n.node_type === 'floor');
  const shutoffNodes = nodes.filter((n: any) => n.node_type === 'shutoff');
  const structNodes = nodes.filter((n: any) => n.node_type === 'structural');

  return (
    <ScrollView
      style={styles.container}
      refreshControl={<RefreshControl refreshing={refreshing} onRefresh={refresh} />}
    >
      {/* System Status Card */}
      <Card style={styles.card}>
        <Card.Content>
          <View style={styles.statusHeader}>
            <Title>System Status</Title>
            <Badge
              size={16}
              style={{ backgroundColor: onlineNodes.length === nodes.length ? '#4caf50' : '#ff9800' }}
            >
              {onlineNodes.length}/{nodes.length}
            </Badge>
          </View>
          <View style={styles.nodeGrid}>
            <View style={styles.nodeStat}>
              <Text style={styles.nodeCount}>{floorNodes.length}</Text>
              <Text style={styles.nodeLabel}>Floor Nodes</Text>
            </View>
            <View style={styles.nodeStat}>
              <Text style={styles.nodeCount}>{shutoffNodes.length}</Text>
              <Text style={styles.nodeLabel}>Shutoff</Text>
            </View>
            <View style={styles.nodeStat}>
              <Text style={styles.nodeCount}>{structNodes.length}</Text>
              <Text style={styles.nodeLabel}>Struct Tags</Text>
            </View>
          </View>
        </Card.Content>
      </Card>

      {/* Latest Event Card */}
      {latestEvent && (
        <Card style={styles.card}>
          <Card.Content>
            <Title>Latest Event</Title>
            <Paragraph>
              Magnitude: M{latestEvent.magnitude.toFixed(1)}
            </Paragraph>
            <Paragraph>
              Severity: {['None', 'Minor', 'Moderate', 'Major', 'Severe'][latestEvent.severity]}
            </Paragraph>
            <Paragraph>
              Actions: {latestEvent.actions_taken & 1 ? '✅ Gas' : '⬜ Gas'}{' '}
              {latestEvent.actions_taken & 2 ? '✅ Water' : '⬜ Water'}
            </Paragraph>
            <Paragraph>
              Time: {new Date(latestEvent.timestamp).toLocaleString()}
            </Paragraph>
          </Card.Content>
        </Card>
      )}

      {/* Node List */}
      <Card style={styles.card}>
        <Card.Content>
          <Title>Nodes ({nodes.length})</Title>
          {nodes.map((node, i) => (
            <View key={i} style={styles.nodeRow}>
              <Text style={styles.nodeAddr}>
                0x{node.node_addr.toString(16).toUpperCase()}
              </Text>
              <Text style={styles.nodeType}>{node.node_type}</Text>
              <Text style={styles.nodeBattery}>{node.battery_pct}%</Text>
              <View style={[
                styles.statusDot,
                { backgroundColor: node.status === 'online' ? '#4caf50' : '#f44336' }
              ]} />
            </View>
          ))}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 8, elevation: 2 },
  statusHeader: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  nodeGrid: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 10 },
  nodeStat: { alignItems: 'center' },
  nodeCount: { fontSize: 24, fontWeight: 'bold' },
  nodeLabel: { fontSize: 12, color: '#666' },
  nodeRow: { flexDirection: 'row', alignItems: 'center', paddingVertical: 6 },
  nodeAddr: { flex: 1, fontFamily: 'monospace', fontSize: 14 },
  nodeType: { flex: 1, fontSize: 12, color: '#666' },
  nodeBattery: { flex: 1, fontSize: 12, color: '#666' },
  statusDot: { width: 10, height: 10, borderRadius: 5, marginLeft: 8 },
});