// SeizureSync — Risk Forecast screen
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { LineChart } from 'react-native-chart-kit';
import axios from 'axios';

const API_BASE = 'https://api.seizuresync.com';

export default function RiskScreen() {
  const [riskData, setRiskData] = useState<number[]>([]);

  useEffect(() => {
    // Fetch 7-day risk history
    axios.get(`${API_BASE}/patients/me/risk?history=7d`).then(r => {
      setRiskData(r.data.history || [10, 15, 25, 30, 20, 45, 35]);
    });
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.header}>24-Hour Risk Forecast</Text>
      {riskData.length > 0 && (
        <LineChart
          data={{ labels: ['6d', '5d', '4d', '3d', '2d', '1d', 'Now'],
                  datasets: [{ data: riskData }] }}
          width={350} height={220}
          chartConfig={{
            backgroundGradientFrom: '#0A1144',
            backgroundGradientTo: '#1a2266',
            color: (opacity=1) => `rgba(255, 200, 0, ${opacity})`,
            labelColor: (opacity=1) => `rgba(255,255,255,${opacity})`,
          }}
          bezier style={{ marginVertical: 8, borderRadius: 16 }}
        />
      )}
      <Text style={styles.note}>
        Risk is forecast by RiskNet (LSTM) from 72-hour multi-signal history.
        Higher values indicate greater seizure likelihood in the next 24 hours.
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5', padding: 16 },
  header: { fontSize: 24, fontWeight: 'bold', color: '#0A1144', marginBottom: 16 },
  note: { fontSize: 12, color: '#666', marginTop: 16 },
});