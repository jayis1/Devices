// PawSync API Client + WebSocket
// Handles all communication with the PawSync cloud backend

const API_BASE = 'https://api.pawsync.local/api/v1';
const WS_BASE = 'wss://api.pawsync.local/api/v1/ws/alerts';

export interface Vitals {
  hr: number;
  hrv_ms: number;
  temp_c: number;
  activity: number;
}

export interface WellnessData {
  wellness: number;
  illness_risk: number;
  anxiety_level: number;
  trend: Array<{ ts: string; wellness: number; illness_risk: number; anxiety: number }>;
}

export interface ActivityEntry {
  ts: string;
  activity: string;
  hr: number;
  hrv: number;
}

export interface FeedingEntry {
  ts: string;
  dispensed_g: number;
  consumed_g: number;
  water_ml: number;
  hopper_pct: number;
  appetite_loss: boolean;
}

export interface AnxietyEpisode {
  ts: string;
  duration_s: number;
  behavior: string;
  vocalization: number;
}

export interface AlertEntry {
  id: number;
  ts: string;
  type: string;
  severity: string;
  message: string;
  acknowledged: boolean;
}

export interface VetReport {
  pet_id: number;
  generated_at: string;
  baseline: { established: boolean; baseline_hr: number; baseline_hrv_ms: number };
  current_vitals: Vitals;
  wellness: WellnessData;
  feeding_summary: { recent_meals: FeedingEntry[]; appetite_loss_count: number };
  anxiety_episodes: AnxietyEpisode[];
  recent_alerts: AlertEntry[];
}

// ---- API calls ----

export async function getWellness(petId: number): Promise<WellnessData> {
  const res = await fetch(`${API_BASE}/pet/${petId}/wellness`);
  return res.json();
}

export async function getActivity(petId: number): Promise<ActivityEntry[]> {
  const res = await fetch(`${API_BASE}/pet/${petId}/activity`);
  return res.json();
}

export async function getVitals(petId: number) {
  const res = await fetch(`${API_BASE}/pet/${petId}/vitals`);
  return res.json();
}

export async function getFeeding(petId: number): Promise<FeedingEntry[]> {
  const res = await fetch(`${API_BASE}/pet/${petId}/feeding`);
  return res.json();
}

export async function getAnxiety(petId: number): Promise<AnxietyEpisode[]> {
  const res = await fetch(`${API_BASE}/pet/${petId}/anxiety`);
  return res.json();
}

export async function getAlerts(petId: number): Promise<AlertEntry[]> {
  const res = await fetch(`${API_BASE}/pet/${petId}/alerts`);
  return res.json();
}

export async function getVetReport(petId: number): Promise<VetReport> {
  const res = await fetch(`${API_BASE}/vet/report/${petId}`, { method: 'POST' });
  return res.json();
}

export async function acknowledgeAlert(petId: number, alertId: number) {
  // POST to acknowledge endpoint (when implemented)
  console.log('Acknowledging alert', alertId, 'for pet', petId);
}

// ---- WebSocket for real-time alerts ----

export class AlertWebSocket {
  private ws: WebSocket | null = null;
  private petId: number;
  private onAlert: (alert: any) => void;

  constructor(petId: number, onAlert: (alert: any) => void) {
    this.petId = petId;
    this.onAlert = onAlert;
  }

  connect() {
    this.ws = new WebSocket(`${WS_BASE}/${this.petId}`);
    this.ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      this.onAlert(data);
    };
    this.ws.onclose = () => {
      // Reconnect after 5s
      setTimeout(() => this.connect(), 5000);
    };
  }

  disconnect() {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}