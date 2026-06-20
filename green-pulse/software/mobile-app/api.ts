// GreenPulse API Client + WebSocket
// Handles all communication with the GreenPulse cloud backend

const API_BASE = 'https://api.greenpulse.local/api/v1';
const WS_BASE = 'wss://api.greenpulse.local/api/v1/ws/alerts';

export interface Plant {
  id: number;
  tag_id: number;
  name: string;
  species: string;
  location: string;
  auto_water: boolean;
  soil_moisture: number | null;
  battery_pct: number | null;
  status: number;       // 0=ok 1=water_soon 2=water_now 3=low_light 4=disease 5=stress
  disease_risk: number;
  water_risk: number;
  hours_to_water: number;
}

export interface Telemetry {
  ts: string;
  soil: number;
  lux: number;
  temp_c: number;
  humidity: number;
  batt: number;
}

export interface WateringEntry {
  ts: string;
  source: string;
  ml: number;
  duration_s: number;
  status: number;
  pre_moisture: number;
}

export interface ScanEntry {
  ts: string;
  disease: string;
  disease_conf: number;
  pests: number;
  image_url: string;
}

export interface RiskData {
  current: {
    disease_risk: number;
    water_risk: number;
    light_risk: number;
    status: number;
    hours_to_water: number;
  } | null;
  trend: Array<{ ts: string; disease: number; water: number; light: number }>;
}

export interface AlertEntry {
  id: number;
  ts: string;
  type: string;
  severity: string;
  message: string;
  plant_id: number;
  acknowledged: boolean;
}

export const STATUS_NAMES = [
  'All Good', 'Water Soon', 'Water Now', 'Low Light', 'Disease', 'Stress'
];

export const STATUS_COLORS = [
  '#4CAF50', '#FF9800', '#F44336', '#2196F3', '#9C27B0', '#FF5722'
];

// ---- API calls ----

export async function getPlants(userId: number): Promise<Plant[]> {
  const res = await fetch(`${API_BASE}/plants/${userId}`);
  return res.json();
}

export async function getTelemetry(plantId: number): Promise<Telemetry[]> {
  const res = await fetch(`${API_BASE}/plant/${plantId}/telemetry`);
  return res.json();
}

export async function getWatering(plantId: number): Promise<WateringEntry[]> {
  const res = await fetch(`${API_BASE}/plant/${plantId}/watering`);
  return res.json();
}

export async function getScans(plantId: number): Promise<ScanEntry[]> {
  const res = await fetch(`${API_BASE}/plant/${plantId}/scans`);
  return res.json();
}

export async function getRisk(plantId: number): Promise<RiskData> {
  const res = await fetch(`${API_BASE}/plant/${plantId}/risk`);
  return res.json();
}

export async function triggerWatering(plantId: number): Promise<any> {
  const res = await fetch(`${API_BASE}/plant/${plantId}/water`, { method: 'POST' });
  return res.json();
}

export async function getAlerts(userId: number): Promise<AlertEntry[]> {
  const res = await fetch(`${API_BASE}/user/${userId}/alerts`);
  return res.json();
}

export async function getSpeciesInfo(speciesId: number): Promise<any> {
  const res = await fetch(`${API_BASE}/species/${speciesId}`);
  return res.json();
}

export async function createPlant(userId: number, tagId: number, name: string,
                                   species: string, profileId: number,
                                   location: string): Promise<any> {
  const res = await fetch(`${API_BASE}/plants?user_id=${userId}&tag_id=${tagId}` +
    `&name=${encodeURIComponent(name)}&species_name=${encodeURIComponent(species)}` +
    `&profile_id=${profileId}&location=${encodeURIComponent(location)}`,
    { method: 'POST' });
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