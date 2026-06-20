// CalmGrid API Client + WebSocket
// Handles all communication with the CalmGrid cloud backend

const API_BASE = 'https://api.calmgrid.local/api/v1';
const WS_BASE = 'wss://api.calmgrid.local/api/v1/ws/alerts';

export interface Vitals {
  hr: number;
  hrv_ms: number;
  eda_scl: number;
  eda_scr: number;
  temp_c: number;
  activity: number;
  steps: number;
}

export interface StressData {
  current: { stress: number; burnout_risk: number; recovery: number } | null;
  trend: Array<{ ts: string; stress: number; burnout: number; recovery: number }>;
}

export interface Episode {
  ts: string;
  hr: number;
  hrv_ms: number;
  eda_scr: number;
  activity: number;
}

export interface InterventionEntry {
  ts: string;
  type: number;
  duration_s: number;
  efficacy: number;
  hrv_delta: number;
}

export interface BurnoutData {
  risk: number;
  current_stress: number;
  avg_stress_30d: number;
  trend: Array<{ ts: string; burnout: number }>;
}

export interface AlertEntry {
  id: number;
  ts: string;
  type: string;
  severity: string;
  message: string;
  acknowledged: boolean;
}

export interface TherapistReport {
  user_id: number;
  generated_at: string;
  summary: {
    avg_hrv_24h: number;
    avg_stress_30d: number;
    burnout_risk: number;
    acute_stress_episodes: number;
  };
  vitals_trend: Array<{ ts: string; hrv_ms: number; eda_scr: number }>;
  stress_trend: Array<{ ts: string; stress: number; burnout: number }>;
  interventions: Array<{ ts: string; type: number; efficacy: number }>;
  recent_alerts: Array<{ ts: string; type: string; severity: string; message: string }>;
}

// ---- API calls ----

export async function getStress(userId: number): Promise<StressData> {
  const res = await fetch(`${API_BASE}/user/${userId}/stress`);
  return res.json();
}

export async function getVitals(userId: number): Promise<Vitals[]> {
  const res = await fetch(`${API_BASE}/user/${userId}/vitals`);
  return res.json();
}

export async function getEpisodes(userId: number): Promise<Episode[]> {
  const res = await fetch(`${API_BASE}/user/${userId}/episodes`);
  return res.json();
}

export async function getBurnout(userId: number): Promise<BurnoutData> {
  const res = await fetch(`${API_BASE}/user/${userId}/burnout`);
  return res.json();
}

export async function getInterventions(userId: number): Promise<InterventionEntry[]> {
  const res = await fetch(`${API_BASE}/user/${userId}/interventions`);
  return res.json();
}

export async function getAlerts(userId: number): Promise<AlertEntry[]> {
  const res = await fetch(`${API_BASE}/user/${userId}/alerts`);
  return res.json();
}

export async function getTherapistReport(userId: number): Promise<TherapistReport> {
  const res = await fetch(`${API_BASE}/therapist/report/${userId}`, { method: 'POST' });
  return res.json();
}

// ---- WebSocket for real-time alerts ----

export class AlertWebSocket {
  private ws: WebSocket | null = null;
  private userId: number;
  private onAlert: (alert: any) => void;

  constructor(userId: number, onAlert: (alert: any) => void) {
    this.userId = userId;
    this.onAlert = onAlert;
  }

  connect() {
    this.ws = new WebSocket(`${WS_BASE}/${this.userId}`);
    this.ws.onmessage = (e) => {
      try {
        const alert = JSON.parse(e.data);
        this.onAlert(alert);
      } catch (err) {
        console.error('WS parse error:', err);
      }
    };
    this.ws.onerror = (e) => console.error('WS error:', e);
  }

  disconnect() {
    this.ws?.close();
    this.ws = null;
  }
}