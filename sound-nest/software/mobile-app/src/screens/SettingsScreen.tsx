import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Switch,
  Slider,
  Alert,
  TextInput,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';

const API_BASE = 'http://soundnest-hub.local:8000/api/v1';

interface NodeConfig {
  node_id: string;
  name: string;
  room: string;
  enabled: boolean;
  spl_cal_offset: number;
  alert_threshold_dba: number;
  dose_threshold_pct: number;
  masking_auto: boolean;
  privacy_mode: boolean;
}

export default function SettingsScreen() {
  const navigation = useNavigation<NativeStackNavigationProp<any>>();
  const [nodes, setNodes] = useState<NodeConfig[]>([]);
  const [loading, setLoading] = useState(true);
  const [globalConfig, setGlobalConfig] = useState({
    hub_name: 'SoundNest Hub',
    wifi_ssid: '',
    timezone: 'UTC',
    dose_limit_pct: 100,
    masking_auto: true,
    privacy_mode: false,
    event_retention_days: 90,
    spl_sample_rate: 16000,
    ml_confidence_threshold: 50,
  });

  useEffect(() => {
    fetchConfig();
  }, []);

  const fetchConfig = async () => {
    try {
      const response = await fetch(`${API_BASE}/config`);
      if (response.ok) {
        const config = await response.json();
        setGlobalConfig(prev => ({ ...prev, ...config }));
      }

      const nodesResponse = await fetch(`${API_BASE}/nodes`);
      if (nodesResponse.ok) {
        const nodeList = await nodesResponse.json();
        setNodes(nodeList);
      }
    } catch (error) {
      console.error('Failed to fetch config:', error);
    } finally {
      setLoading(false);
    }
  };

  const saveConfig = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/config`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(globalConfig),
      });
      if (response.ok) {
        Alert.alert('Success', 'Configuration saved');
      } else {
        Alert.alert('Error', 'Failed to save configuration');
      }
    } catch (error) {
      Alert.alert('Error', 'Failed to connect to hub');
    }
  }, [globalConfig]);

  const handleNodeToggle = useCallback((nodeId: string, enabled: boolean) => {
    setNodes(prev =>
      prev.map(n => n.node_id === nodeId ? { ...n, enabled } : n)
    );
  }, []);

  const handleFactoryReset = useCallback(() => {
    Alert.alert(
      'Factory Reset',
      'This will erase all settings and paired nodes. Continue?',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Reset',
          style: 'destructive',
          onPress: async () => {
            try {
              await fetch(`${API_BASE}/config`, { method: 'DELETE' });
              Alert.alert('Reset Complete', 'Hub will restart...');
            } catch (error) {
              Alert.alert('Error', 'Failed to reset');
            }
          },
        },
      ]
    );
  }, []);

  return (
    <ScrollView style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.title}>Settings</Text>
      </View>

      {/* Hub Configuration */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Hub Configuration</Text>

        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>Hub Name</Text>
          <TextInput
            style={styles.textInput}
            value={globalConfig.hub_name}
            onChangeText={text =>
              setGlobalConfig(prev => ({ ...prev, hub_name: text }))
            }
          />
        </View>

        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>WiFi SSID</Text>
          <TextInput
            style={styles.textInput}
            value={globalConfig.wifi_ssid}
            onChangeText={text =>
              setGlobalConfig(prev => ({ ...prev, wifi_ssid: text }))
            }
            placeholder="Enter WiFi SSID"
          />
        </View>

        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>Timezone</Text>
          <TextInput
            style={styles.textInput}
            value={globalConfig.timezone}
            onChangeText={text =>
              setGlobalConfig(prev => ({ ...prev, timezone: text }))
            }
          />
        </View>

        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>Event Retention (days)</Text>
          <TextInput
            style={styles.textInput}
            value={String(globalConfig.event_retention_days)}
            onChangeText={text =>
              setGlobalConfig(prev => ({
                ...prev,
                event_retention_days: parseInt(text) || 90,
              }))
            }
            keyboardType="numeric"
          />
        </View>
      </View>

      {/* Acoustic Settings */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Acoustic Settings</Text>

        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>Daily Dose Limit (%)</Text>
          <Text style={styles.settingValue}>{globalConfig.dose_limit_pct}%</Text>
        </View>
        <Slider
          style={styles.slider}
          minimumValue={50}
          maximumValue={200}
          step={10}
          value={globalConfig.dose_limit_pct}
          onValueChange={value =>
            setGlobalConfig(prev => ({ ...prev, dose_limit_pct: value }))
          }
          minimumTrackTintColor="#4ECDC4"
          maximumTrackTintColor="#555"
          thumbTintColor="#4ECDC4"
        />

        <View style={styles.settingRow}>
          <Text style={styles.settingLabel}>ML Confidence Threshold (%)</Text>
          <Text style={styles.settingValue}>
            {globalConfig.ml_confidence_threshold}%
          </Text>
        </View>
        <Slider
          style={styles.slider}
          minimumValue={10}
          maximumValue={95}
          step={5}
          value={globalConfig.ml_confidence_threshold}
          onValueChange={value =>
            setGlobalConfig(prev => ({ ...prev, ml_confidence_threshold: value }))
          }
          minimumTrackTintColor="#4ECDC4"
          maximumTrackTintColor="#555"
          thumbTintColor="#4ECDC4"
        />

        <View style={styles.switchRow}>
          <Text style={styles.settingLabel}>Auto Masking</Text>
          <Switch
            value={globalConfig.masking_auto}
            onValueChange={value =>
              setGlobalConfig(prev => ({ ...prev, masking_auto: value }))
            }
            trackColor={{ false: '#555', true: '#4ECDC4' }}
            thumbColor="#fff"
          />
        </View>

        <View style={styles.switchRow}>
          <Text style={styles.settingLabel}>Privacy Mode</Text>
          <Switch
            value={globalConfig.privacy_mode}
            onValueChange={value =>
              setGlobalConfig(prev => ({ ...prev, privacy_mode: value }))
            }
            trackColor={{ false: '#555', true: '#4ECDC4' }}
            thumbColor="#fff"
          />
        </View>
      </View>

      {/* Node Management */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Node Management</Text>

        {nodes.map(node => (
          <View key={node.node_id} style={styles.nodeRow}>
            <View style={styles.nodeInfo}>
              <Text style={styles.nodeName}>{node.name || node.node_id}</Text>
              <Text style={styles.nodeRoom}>{node.room}</Text>
            </View>
            <Switch
              value={node.enabled}
              onValueChange={value =>
                handleNodeToggle(node.node_id, value)
              }
              trackColor={{ false: '#555', true: '#4ECDC4' }}
              thumbColor="#fff"
            />
          </View>
        ))}

        <TouchableOpacity
          style={styles.pairButton}
          onPress={() => {
            Alert.alert(
              'Pair New Node',
              'Put the node in pairing mode by pressing its setup button for 5 seconds, then tap Search.'
            );
          }}
        >
          <Text style={styles.pairButtonText}>+ Pair New Node</Text>
        </TouchableOpacity>
      </View>

      {/* Actions */}
      <View style={styles.section}>
        <TouchableOpacity
          style={styles.saveButton}
          onPress={saveConfig}
        >
          <Text style={styles.saveButtonText}>Save Configuration</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.dangerButton}
          onPress={handleFactoryReset}
        >
          <Text style={styles.dangerButtonText}>Factory Reset</Text>
        </TouchableOpacity>
      </View>

      {/* About */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>About</Text>
        <Text style={styles.aboutText}>SoundNest v1.0.0</Text>
        <Text style={styles.aboutText}>AI-Powered Home Acoustic Intelligence</Text>
        <Text style={styles.aboutText}>© 2024 SoundNest Project</Text>
      </View>

      <View style={styles.spacer} />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0D1B2A',
  },
  header: {
    padding: 20,
    paddingTop: 60,
    backgroundColor: '#1B2838',
  },
  title: {
    fontSize: 28,
    fontWeight: '700',
    color: '#FFFFFF',
  },
  section: {
    margin: 16,
    padding: 16,
    backgroundColor: '#1B2838',
    borderRadius: 12,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#4ECDC4',
    marginBottom: 12,
  },
  settingRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#2A3A4A',
  },
  settingLabel: {
    fontSize: 16,
    color: '#E0E0E0',
    flex: 1,
  },
  settingValue: {
    fontSize: 16,
    color: '#4ECDC4',
    fontWeight: '600',
  },
  switchRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
  },
  textInput: {
    fontSize: 16,
    color: '#E0E0E0',
    backgroundColor: '#2A3A4A',
    borderRadius: 8,
    padding: 8,
    flex: 1,
    marginLeft: 12,
  },
  slider: {
    marginVertical: 4,
  },
  nodeRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#2A3A4A',
  },
  nodeInfo: {
    flex: 1,
  },
  nodeName: {
    fontSize: 16,
    color: '#E0E0E0',
    fontWeight: '500',
  },
  nodeRoom: {
    fontSize: 12,
    color: '#8A9BA8',
  },
  pairButton: {
    backgroundColor: '#4ECDC4',
    borderRadius: 8,
    padding: 12,
    alignItems: 'center',
    marginTop: 12,
  },
  pairButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#0D1B2A',
  },
  saveButton: {
    backgroundColor: '#4ECDC4',
    borderRadius: 8,
    padding: 16,
    alignItems: 'center',
    marginBottom: 12,
  },
  saveButtonText: {
    fontSize: 18,
    fontWeight: '700',
    color: '#0D1B2A',
  },
  dangerButton: {
    backgroundColor: '#FF6B6B',
    borderRadius: 8,
    padding: 16,
    alignItems: 'center',
  },
  dangerButtonText: {
    fontSize: 18,
    fontWeight: '700',
    color: '#FFFFFF',
  },
  aboutText: {
    fontSize: 14,
    color: '#8A9BA8',
    textAlign: 'center',
    marginBottom: 4,
  },
  spacer: {
    height: 40,
  },
});