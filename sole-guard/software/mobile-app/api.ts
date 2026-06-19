/**
 * api.ts — SoleGuard REST + WebSocket client
 */

const API_BASE = 'http://localhost:8000/api/v1';

export const api = {
  getRisk: async (pid: number) => {
    const r = await fetch(`${API_BASE}/patient/${pid}/risk`);
    return r.json();
  },
  getHeatmap: async (pid: number) => {
    const r = await fetch(`${API_BASE}/patient/${pid}/heatmap`);
    return r.json();
  },
  getGait: async (pid: number) => {
    const r = await fetch(`${API_BASE}/patient/${pid}/gait`);
    return r.json();
  },
  getScans: async (pid: number) => {
    const r = await fetch(`${API_BASE}/patient/${pid}/scans`);
    return r.json();
  },
  getAlerts: async (pid: number) => {
    const r = await fetch(`${API_BASE}/patient/${pid}/alerts`);
    return r.json();
  },
  acknowledgeAlert: async (pid: number, id: number | string) => {
    await fetch(`${API_BASE}/patient/${pid}/alerts/${id}/ack`, { method: 'POST' });
  },
  sendClinicianReport: async (pid: number) => {
    const r = await fetch(`${API_BASE}/clinician/report/${pid}`, { method: 'POST' });
    return r.json();
  },
  connectAlerts: (pid: number, onAlert: (a: any) => void) => {
    const ws = new WebSocket(`ws://localhost:8000/api/v1/ws/alerts/${pid}`);
    ws.onmessage = (e) => {
      try { onAlert(JSON.parse(e.data)); } catch {}
    };
    return ws;
  },
};