/**
 * PowerPulse — useAlerts Hook
 */

import { useState, useEffect, useCallback } from 'react';

const API_BASE = 'http://powerpulse.local:8000/api/v1';

interface Alert {
  id: number;
  timestamp: string;
  alert_type: string;
  severity: number;
  circuit_id?: number;
  tag_id?: number;
  message: string;
  confidence: number;
  acknowledged: boolean;
  resolved: boolean;
}

export function useAlerts() {
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [unreadCount, setUnreadCount] = useState(0);

  const fetchAlerts = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/alerts?limit=50`);
      if (response.ok) {
        const data: Alert[] = await response.json();
        setAlerts(data);
        setUnreadCount(data.filter(a => !a.acknowledged).length);
      }
    } catch (error) {
      console.error('Failed to fetch alerts:', error);
    }
  }, []);

  const acknowledge = useCallback(async (alertId: number) => {
    try {
      await fetch(`${API_BASE}/alerts/${alertId}/acknowledge`, { method: 'POST' });
      fetchAlerts();
    } catch (error) {
      console.error('Failed to acknowledge alert:', error);
    }
  }, [fetchAlerts]);

  useEffect(() => {
    fetchAlerts();
    const interval = setInterval(fetchAlerts, 10000);
    return () => clearInterval(interval);
  }, [fetchAlerts]);

  return { alerts, unreadCount, acknowledge, refresh: fetchAlerts };
}