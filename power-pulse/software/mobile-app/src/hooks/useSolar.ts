/**
 * PowerPulse — useSolar Hook
 */

import { useState, useEffect, useCallback } from 'react';

const API_BASE = 'http://powerpulse.local:8000/api/v1';

interface SolarData {
  pv_voltage_mv: number;
  pv_current_ma: number;
  pv_power_w: number;
  batt_voltage_mv: number;
  load_current_ma: number;
  load_power_w: number;
  soc_pct: number;
  charge_mode: number;
  mppt_duty_pct: number;
  heatsink_temp_c: number;
  fan_speed_pct: number;
  energy_produced_wh: number;
  energy_consumed_wh: number;
}

export function useSolar() {
  const [solar, setSolar] = useState<SolarData | null>(null);
  const [loading, setLoading] = useState(true);

  const fetchSolar = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/solar/battery`);
      if (response.ok) {
        const data = await response.json();
        setSolar(data);
      }
    } catch (error) {
      console.error('Failed to fetch solar data:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchSolar();
    const interval = setInterval(fetchSolar, 5000);
    return () => clearInterval(interval);
  }, [fetchSolar]);

  return { solar, loading, refresh: fetchSolar };
}