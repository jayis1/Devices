// SeizureSync — API client helper
import axios from 'axios';

const API_BASE = 'https://api.seizuresync.com';

export const api = axios.create({ baseURL: API_BASE });

export async function getRisk() {
  const r = await api.get('/patients/me/risk');
  return r.data;
}

export async function getEvents(limit = 50) {
  const r = await api.get(`/patients/me/events?limit=${limit}`);
  return r.data;
}

export async function getSUDEP() {
  const r = await api.get('/patients/me/sudep');
  return r.data;
}

export async function ackAlert(alertId: string) {
  await api.post(`/patients/me/alerts/${alertId}/ack`);
}

export async function generateReport() {
  const r = await api.post('/patients/me/reports', {}, { responseType: 'blob' });
  return r.data;
}