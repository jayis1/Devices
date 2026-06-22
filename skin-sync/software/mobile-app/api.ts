// SkinSync API Client + WebSocket
// Handles all communication with the SkinSync cloud backend

const API_BASE = 'https://api.skinsync.local/api/v1';
const WS_BASE = 'wss://api.skinsync.local/api/v1/ws/alerts';

export interface UVEvent {
  ts: string;
  uva: number;
  uvb: number;
  med_frac: number;
  uv_idx: number;
  temp_c: number;
  uv_status: number;
}

export interface ScanEntry {
  ts: string;
  condition: string;
  conf: number;
  abcde: number;
  skin_age: number;
  lesion_id: number;
  image_url: string;
}

export interface DispenseEntry {
  ts: string;
  slot: number;
  product: string;
  mg: number;
  remaining: number;
  status: number;
}

export interface LesionEntry {
  lesion_id: number;
  location: number;
  first_seen: string;
  last_scanned: string;
  abcde: number;
  status: string;
}

export interface RiskData {
  current: {
    uv_burn_risk: number;
    skin_cancer_risk: number;
    skin_status: number;
    skin_age: number;
    routine_score: number;
  } | null;
  trend: Array<{ ts: string; burn: number; cancer: number; skin_age: number }>;
}

export interface AlertEntry {
  id: number;
  ts: string;
  type: string;
  severity: string;
  message: string;
  acknowledged: boolean;
}

export interface InventoryItem {
  slot: number;
  product: string;
  remaining_pct: number;
  mg_remaining: number;
}

export const UV_STATUS_NAMES = ['Safe', 'Caution', 'Warning', 'Danger', 'Burned'];
export const UV_STATUS_COLORS = ['#4CAF50', '#FF9800', '#FF5722', '#F44336', '#B71C1C'];

// ---- API calls ----

export async function getUVHistory(userId: number, hours = 24): Promise<UVEvent[]> {
  const res = await fetch(`${API_BASE}/users/${userId}/uv?hours=${hours}`);
  return res.json();
}

export async function getScans(userId: number): Promise<ScanEntry[]> {
  const res = await fetch(`${API_BASE}/users/${userId}/scans`);
  return res.json();
}

export async function getLesions(userId: number): Promise<LesionEntry[]> {
  const res = await fetch(`${API_BASE}/users/${userId}/lesions`);
  return res.json();
}

export async function getDispenseHistory(userId: number): Promise<DispenseEntry[]> {
  const res = await fetch(`${API_BASE}/users/${userId}/dispense`);
  return res.json();
}

export async function getRisk(userId: number): Promise<RiskData> {
  const res = await fetch(`${API_BASE}/users/${userId}/risk`);
  return res.json();
}

export async function getAlerts(userId: number): Promise<AlertEntry[]> {
  const res = await fetch(`${API_BASE}/users/${userId}/alerts`);
  return res.json();
}

export async function getInventory(userId: number): Promise<InventoryItem[]> {
  const res = await fetch(`${API_BASE}/users/${userId}/inventory`);
  return res.json();
}

export async function triggerDispense(userId: number, slot: number, amountMg: number): Promise<any> {
  const res = await fetch(`${API_BASE}/users/${userId}/dispense?slot=${slot}&amount_mg=${amountMg}`,
    { method: 'POST' });
  return res.json();
}

export async function getDermReport(userId: number): Promise<any> {
  const res = await fetch(`${API_BASE}/users/${userId}/derm-report`);
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