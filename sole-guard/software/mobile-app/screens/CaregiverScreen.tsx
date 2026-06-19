/**
 * CaregiverScreen — add caregivers, share risk + alerts, send clinician report
 */

import React, { useState } from 'react';
import { View, StyleSheet, ScrollView } from 'react-native';
import { Text, Card, Title, Paragraph, TextInput, Button, List } from 'react-native-paper';
import { api } from '../api';

export default function CaregiverScreen() {
  const [caregivers, setCaregivers] = useState([
    { name: 'Dr. Patel (Clinician)', email: 'patel@clinic.org', type: 'clinician' },
    { name: 'Mary (Daughter)', email: 'mary@email.com', type: 'caregiver' },
  ]);
  const [newName, setNewName] = useState('');
  const [newEmail, setNewEmail] = useState('');
  const patientId = 1;

  const addCaregiver = () => {
    if (!newName || !newEmail) return;
    setCaregivers([...caregivers, { name: newName, email: newEmail, type: 'caregiver' }]);
    setNewName(''); setNewEmail('');
  };

  const sendReport = () => {
    api.sendClinicianReport(patientId).then(() =>
      alert('Clinician report sent to Dr. Patel.')
    );
  };

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Shared With</Title>
          {caregivers.map((c, i) => (
            <List.Item
              key={i}
              title={c.name}
              description={c.email}
              left={(props) => <List.Icon {...props} icon={c.type === 'clinician' ? 'doctor' : 'account'} />}
            />
          ))}
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Add Caregiver</Title>
          <TextInput label="Name" value={newName} onChangeText={setNewName} style={styles.input} />
          <TextInput label="Email" value={newEmail} onChangeText={setNewEmail}
            keyboardType="email-address" style={styles.input} />
          <Button mode="contained" onPress={addCaregiver}>Add</Button>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Clinician Report</Title>
          <Paragraph>Send a structured wound + risk report to your clinician.</Paragraph>
          <Button mode="contained" color="#e91e63" onPress={sendReport} style={styles.sendBtn}>
            Send Report Now
          </Button>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  card: { margin: 12 },
  input: { marginVertical: 8 },
  sendBtn: { marginTop: 8 },
});