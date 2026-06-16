/**
 * PowerPulse — useEnergy Hook
 * 
 * Custom React hook for fetching real-time energy data from the API.
 */

import { useState, useEffect, useCallback } from 'react';

const API_BASE = 'http://powerpulse.local:8000/api/v1';

interface PowerFlow {
  total_consumption_w: number;
  solar_production_w: number;
  grid_import_w: number;
  grid_export_w: number;
  battery_charge_w: number;
  battery_soc_pct: number;
  circuit_breakdown: Record<string, number>;
  appliance_breakdown: Record<string, number>;
}

interface CircuitReading {
  timestamp: string;
  circuit_id: number;
  voltage_mv: number;
  current_ma: number;
  power_w: number;
  power_factor: number;
  energy_wh: number;
}

export function useEnergy() {
  const [powerFlow, setPowerFlow] = useState<PowerFlow | null>(null);
  const [circuits, setCircuits] = useState<CircuitReading[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchPowerFlow = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/energy/total`);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      setPowerFlow(data);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    }
  }, []);

  const fetchCircuits = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/energy/circuits?limit=20`);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      setCircuits(data);
    } catch (err) {
      console.error('Failed to fetch circuits:', err);
    }
  }, []);

  const refresh = useCallback(async () => {
    setLoading(true);
    await Promise.all([fetchPowerFlow(), fetchCircuits()]);
    setLoading(false);
  }, [fetchPowerFlow, fetchCircuits]);

  useEffect(() => {
    refresh();
    // Poll every 5 seconds for near-real-time updates
    const interval = setInterval(fetchPowerFlow, 5000);
    return () => clearInterval(interval);
  }, [refresh, fetchPowerFlow]);

  return { powerFlow, circuits, loading, error, refresh };
}