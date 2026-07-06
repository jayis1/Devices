/**
 * SightSync Mobile App — Settings Screen
 * License: MIT
 */

import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { Card, List, Switch, Button, Divider } from 'react-native-paper';
import { useAuth } from '../context/AuthContext';

export default function SettingsScreen() {
  const { logout } = useAuth();
  const [notifications, setNotifications] = React.useState(true);
  const [cloudSync, setCloudSync] = React.useState(true);
  const [myopiaTracking, setMyopiaTracking] = React.useState(false);

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Title title="Notifications" />
        <Card.Content>
          <List.Item
            title="Fatigue Alerts"
            description="Haptic + audio when fatigue >70"
            right={() => (
              <Switch value={notifications} onValueChange={setNotifications} />
            )}
          />
          <Divider />
          <List.Item
            title="20-20-20 Reminders"
            description="Vibration every 20 minutes"
            right={() => (
              <Switch value={notifications} onValueChange={setNotifications} />
            )}
          />
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Title title="Privacy" />
        <Card.Content>
          <List.Item
            title="Cloud Sync"
            description="Send anonymous eye health data to cloud"
            right={() => (
              <Switch value={cloudSync} onValueChange={setCloudSync} />
            )}
          />
          <Divider />
          <List.Item
            title="Myopia Tracking (Children)"
            description="Enable 90-day myopia progression forecast"
            right={() => (
              <Switch value={myopiaTracking} onValueChange={setMyopiaTracking} />
            )}
          />
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Title title="Nodes" />
        <Card.Content>
          <List.Item title="Vision Hub" description="Connected" left={() => <List.Icon icon="router" />} />
          <List.Item title="Desk Sentinel" description="Connected" left={() => <List.Icon icon="monitor" />} />
          <List.Item title="Eye Tag" description="Connected" left={() => <List.Icon icon="glasses" />} />
          <List.Item title="Lamp Node" description="Connected" left={() => <List.Icon icon="lamp" />} />
        </Card.Content>
      </Card>

      <Button mode="outlined" onPress={logout} style={styles.logoutButton}>
        Sign Out
      </Button>

      <Text style={styles.version}>SightSync v1.0.0</Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 8 },
  card: { marginVertical: 8, elevation: 2 },
  logoutButton: { margin: 16 },
  version: { textAlign: 'center', color: '#888', fontSize: 12, marginBottom: 16 },
});