/**
 * JointSync Mobile App — Login Screen
 */

import React, { useState } from 'react';
import { View, Text, TextInput, Button, StyleSheet, Alert } from 'react-native';
import { useAuth } from '../context/AuthContext';

export function LoginScreen() {
  const { login, register } = useAuth();
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [name, setName] = useState('');
  const [diagnosis, setDiagnosis] = useState('oa');
  const [isRegister, setIsRegister] = useState(false);
  const [loading, setLoading] = useState(false);

  const handleSubmit = async () => {
    if (!email || !password) return;
    setLoading(true);
    try {
      if (isRegister) {
        await register(email, name || 'User', password, diagnosis);
      } else {
        await login(email, password);
      }
    } catch (e: any) {
      Alert.alert('Error', e.message || 'Authentication failed');
    } finally {
      setLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.logo}>JointSync</Text>
      <Text style={styles.subtitle}>AI-Powered Joint Health Management</Text>

      {isRegister && (
        <TextInput
          style={styles.input}
          placeholder="Name"
          value={name}
          onChangeText={setName}
        />
      )}

      <TextInput
        style={styles.input}
        placeholder="Email"
        value={email}
        onChangeText={setEmail}
        autoCapitalize="none"
        keyboardType="email-address"
      />

      <TextInput
        style={styles.input}
        placeholder="Password"
        value={password}
        onChangeText={setPassword}
        secureTextEntry
      />

      {isRegister && (
        <TextInput
          style={styles.input}
          placeholder="Diagnosis (ra, oa, psa, other)"
          value={diagnosis}
          onChangeText={setDiagnosis}
        />
      )}

      <Button title={isRegister ? 'Register' : 'Login'} onPress={handleSubmit} disabled={loading} />

      <Button
        title={isRegister ? 'Already have an account? Login' : "Don't have an account? Register"}
        onPress={() => setIsRegister(!isRegister)}
        color="#888"
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, justifyContent: 'center', padding: 30, backgroundColor: '#f5f5f5' },
  logo: { fontSize: 36, fontWeight: 'bold', textAlign: 'center', color: '#0066CC', marginBottom: 8 },
  subtitle: { fontSize: 14, textAlign: 'center', color: '#666', marginBottom: 30 },
  input: { backgroundColor: '#fff', padding: 15, borderRadius: 8, marginBottom: 12, fontSize: 16 },
});