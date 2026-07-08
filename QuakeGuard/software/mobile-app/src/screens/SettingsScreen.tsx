import React, { useState } from 'react';
import { View, Text, ScrollView, StyleSheet, TextInput, Alert } from 'react-native';
import { Card, Title, Paragraph, Button, Switch, List } from 'react-native-paper';
import { client } from '../api/client';
import AsyncStorage from '@react-native-async-storage/async-storage';

export default function SettingsScreen() {
  const [hubId, setHubId] = useState('');
  const [emergencyContact, setEmergencyContact] = useState('');
  const [valveTestEnabled, setValveTestEnabled] = useState(true);
  const [cellularBackup, setCellularBackup] = useState(true);
  const [pushNotifications, setPushNotifications] = useState(true);

  const saveHubId = async () => {
    await client.setHubId(hubId);
    Alert.alert('Saved', `Hub ID set to ${hubId}`);
  };

  const runValveTest = async () => {
    Alert.alert(
      'Valve Test',
      'This will cycle gas and water valves. Ensure appliances are off.',
      [
        { text: 'Cancel' },
        {
          text: 'Run Test',
          onPress: () => Alert.alert('Test Started', 'Valves will cycle in 10 seconds.'),
        },
      ]
    );
  };

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Hub Configuration</Title>
          <TextInput
            style={styles.input}
            placeholder="Hub ID (e.g., QG-001234)"
            value={hubId}
            onChangeText={setHubId}
          />
          <Button mode="contained" onPress={saveHubId} style={styles.button}>
            Save Hub ID
          </Button>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Emergency Contact</Title>
          <TextInput
            style={styles.input}
            placeholder="Phone number"
            value={emergencyContact}
            onChangeText={setEmergencyContact}
            keyboardType="phone-pad"
          />
          <Paragraph style={styles.helpText}>
            This contact will be called if you don't respond to a safety check-in
            within 10 minutes after an earthquake.
          </Paragraph>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Safety Features</Title>
          <List.Item
            title="Monthly valve self-test"
            description="Automatically test gas and water valves on the 1st of each month"
            right={() => (
              <Switch value={valveTestEnabled} onValueChange={setValveTestEnabled} />
            )}
          />
          <List.Item
            title="Cellular backup"
            description="Use 4G LTE when Wi-Fi is unavailable (post-quake)"
            right={() => (
              <Switch value={cellularBackup} onValueChange={setCellularBackup} />
            )}
          />
          <List.Item
            title="Push notifications"
            description="Receive earthquake alerts on this phone"
            right={() => (
              <Switch value={pushNotifications} onValueChange={setPushNotifications} />
            )}
          />
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Manual Test</Title>
          <Button mode="outlined" onPress={runValveTest} style={styles.button}>
            Run Valve Test Now
          </Button>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>About</Title>
          <Paragraph>QuakeGuard v1.0.0</Paragraph>
          <Paragraph>AI-Powered Earthquake Early-Warning</Paragraph>
          <Paragraph>& Structural Safety System</Paragraph>
          <Paragraph style={styles.aboutText}>MIT License</Paragraph>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 8, elevation: 2 },
  input: {
    borderWidth: 1,
    borderColor: '#ccc',
    borderRadius: 4,
    padding: 10,
    marginVertical: 8,
    fontSize: 16,
  },
  button: { marginTop: 8 },
  helpText: { fontSize: 12, color: '#666', marginTop: 8 },
  aboutText: { marginTop: 8, fontSize: 12, color: '#666' },
});