import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, StyleSheet, Alert } from 'react-native';
import { Card, Title, Paragraph, Button, IconButton } from 'react-native-paper';
import { client } from '../api/client';

export default function AlertScreen() {
  const [activeAlert, setActiveAlert] = useState<any | null>(null);
  const [familyStatus, setFamilyStatus] = useState<string>('pending');

  useEffect(() => {
    client.connectWebSocket((data) => {
      if (data.type === 'event' || data.type === 'family_checkin') {
        setActiveAlert(data);
        setFamilyStatus('pending');
      }
    });
    return () => client.disconnect();
  }, []);

  const respond = async (status: 'safe' | 'need_help') => {
    if (!activeAlert) return;
    try {
      await client.sendFamilyResponse(
        activeAlert.event_id,
        'user_1', // In production: from auth context
        status
      );
      setFamilyStatus(status);
    } catch (e) {
      Alert.alert('Error', 'Failed to send response');
    }
  };

  if (!activeAlert) {
    return (
      <View style={styles.noAlert}>
        <IconButton icon="shield-check" size={64} iconColor="#4caf50" />
        <Text style={styles.noAlertText}>No active alerts</Text>
        <Text style={styles.noAlertSubtext}>
          You will be notified immediately when an earthquake is detected
        </Text>
      </View>
    );
  }

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.alertCard}>
        <Card.Content>
          <Title style={styles.alertTitle}>⚠️ EARTHQUAKE DETECTED</Title>
          {activeAlert.magnitude && (
            <Paragraph style={styles.magnitudeText}>
              Magnitude: M{activeAlert.magnitude.toFixed(1)}
            </Paragraph>
          )}
          <Paragraph style={styles.alertText}>
            Protective actions have been taken:
          </Paragraph>
          <View style={styles.actionList}>
            <Text style={styles.actionItem}>
              {activeAlert.actions_taken & 1 ? '✅' : '⬜'} Gas valve closed
            </Text>
            <Text style={styles.actionItem}>
              {activeAlert.actions_taken & 2 ? '✅' : '⬜'} Water valve closed
            </Text>
            <Text style={styles.actionItem}>
              {activeAlert.actions_taken & 4 ? '✅' : '⬜'} Elevator secured
            </Text>
            <Text style={styles.actionItem}>
              {activeAlert.actions_taken & 8 ? '✅' : '⬜'} Awning retracted
            </Text>
          </View>
        </Card.Content>
      </Card>

      {/* Family Safety Check-in */}
      <Card style={styles.checkinCard}>
        <Card.Content>
          <Title>Are you safe?</Title>
          {familyStatus === 'pending' ? (
            <View style={styles.buttonRow}>
              <Button
                mode="contained"
                style={[styles.responseButton, { backgroundColor: '#4caf50' }]}
                onPress={() => respond('safe')}
              >
                ✅ I'm Safe
              </Button>
              <Button
                mode="contained"
                style={[styles.responseButton, { backgroundColor: '#f44336' }]}
                onPress={() => respond('need_help')}
              >
                🆘 Need Help
              </Button>
            </View>
          ) : (
            <View style={styles.respondedContainer}>
              <Text style={styles.respondedText}>
                {familyStatus === 'safe'
                  ? '✅ You reported safe. Emergency contacts notified.'
                  : '🆘 Help request sent. Emergency services contacted.'}
              </Text>
            </View>
          )}
        </Card.Content>
      </Card>

      {/* Safety Instructions */}
      <Card style={styles.card}>
        <Card.Content>
          <Title>Safety Instructions</Title>
          <Text style={styles.instructionText}>
            1. Drop to the ground, take cover under a sturdy table, hold on
          </Text>
          <Text style={styles.instructionText}>
            2. Stay away from windows, mirrors, and heavy furniture
          </Text>
          <Text style={styles.instructionText}>
            3. If outdoors, move to an open area away from buildings
          </Text>
          <Text style={styles.instructionText}>
            4. Do not re-enter the building until cleared by QuakeGuard
          </Text>
          <Text style={styles.instructionText}>
            5. Check gas valves (auto-shutoff activated)
          </Text>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  noAlert: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  noAlertText: { fontSize: 20, fontWeight: 'bold', marginTop: 16 },
  noAlertSubtext: { fontSize: 14, color: '#666', textAlign: 'center', marginTop: 8 },
  alertCard: { margin: 8, backgroundColor: '#fff3e0', elevation: 4 },
  alertTitle: { color: '#e65100', fontSize: 22 },
  magnitudeText: { fontSize: 20, fontWeight: 'bold', marginVertical: 8 },
  alertText: { fontSize: 14, marginTop: 8 },
  actionList: { marginLeft: 8, marginTop: 8 },
  actionItem: { fontSize: 14, paddingVertical: 2 },
  checkinCard: { margin: 8, backgroundColor: '#e3f2fd', elevation: 4 },
  buttonRow: { flexDirection: 'row', justifyContent: 'space-around', marginTop: 12 },
  responseButton: { flex: 1, marginHorizontal: 8 },
  respondedContainer: { marginTop: 12, alignItems: 'center' },
  respondedText: { fontSize: 16, fontWeight: 'bold', textAlign: 'center' },
  card: { margin: 8, elevation: 2 },
  instructionText: { fontSize: 14, paddingVertical: 4 },
});