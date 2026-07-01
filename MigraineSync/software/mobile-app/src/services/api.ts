/**
 * MigraineSync — API Service
 * ===========================
 * REST client for the MigraineSync FastAPI backend.
 *
 * License: MIT
 */

import axios from 'axios';

const API_BASE_URL = 'https://api.migrainesync.io/api/v1';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Add auth token interceptor (in production: JWT from secure storage)
api.interceptors.request.use(async (config) => {
  // const token = await SecureStore.getItemAsync('jwt_token');
  // if (token) config.headers.Authorization = `Bearer ${token}`;
  return config;
});

// ── Types ────────────────────────────────────────────────
export interface RiskForecast {
  risk_score: number;
  risk_level: 'low' | 'moderate' | 'high';
  confidence: number;
  forecast_hours: number;
  contributing_factors: Array<{
    factor: string;
    contribution_pct: number;
    value: string;
  }>;
  trend: string;
  recommended_action: string | null;
  last_updated: string;
}

export interface TriggerAttribution {
  trigger: string;
  contribution_pct: number;
  exposure_level: string;
  recommendation: string;
}

export interface HydrationSummary {
  intake_today_ml: number;
  goal_ml: number;
  pct_of_goal: number;
  sip_count_today: number;
  pattern: string;
  last_sip: string | null;
  trend_7d: number[];
  recommendation: string;
}

export interface ActionPlan {
  zone: 'green' | 'yellow' | 'red';
  risk_score: number;
  steps: string[];
  last_updated: string;
}

export interface EventLog {
  timestamp: string;
  event_type: string;
  severity: number;
  message: string;
}

// ── API Methods ──────────────────────────────────────────

export async function getRisk(): Promise<RiskForecast> {
  const { data } = await api.get<RiskForecast>('/risk');
  return data;
}

export async function getTriggers(): Promise<TriggerAttribution[]> {
  const { data } = await api.get<TriggerAttribution[]>('/triggers');
  return data;
}

export async function getTriggerHeatmap(): Promise<any> {
  const { data } = await api.get('/triggers/heatmap');
  return data;
}

export async function getHydration(): Promise<HydrationSummary> {
  const { data } = await api.get<HydrationSummary>('/hydration');
  return data;
}

export async function getEvents(limit: number = 50): Promise<EventLog[]> {
  const { data } = await api.get<EventLog[]>('/events', { params: { limit } });
  return data;
}

export async function getTrends(metric: string, hours: number = 24): Promise<any> {
  const { data } = await api.get('/trends', { params: { metric, hours } });
  return data;
}

export async function getActionPlan(): Promise<ActionPlan> {
  const { data } = await api.get<ActionPlan>('/action-plan');
  return data;
}

export async function logEvent(eventType: string, value?: number, note?: string): Promise<void> {
  await api.post('/event', {
    event_type: eventType,
    value: value,
    note: note,
  });
}

export async function getReport(): Promise<any> {
  const { data } = await api.get('/report');
  return data;
}

export default api;