import axios from 'axios';

const API_BASE = 'https://api.compostsync.local';

export const api = axios.create({
  baseURL: API_BASE,
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' },
});

export async function getCompostStatus(deviceId: string) {
  const { data } = await api.get(`/api/compost/${deviceId}/status`);
  return data;
}

export async function getTimeline(deviceId: string, days = 30) {
  const { data } = await api.get(`/api/compost/${deviceId}/timeline?days=${days}`);
  return data;
}

export async function getRecipes(deviceId: string) {
  const { data } = await api.get(`/api/compost/${deviceId}/recipes`);
  return data;
}

export async function getAlerts(deviceId: string) {
  const { data } = await api.get(`/api/alerts/${deviceId}`);
  return data;
}

export async function acknowledgeAlert(alertId: number) {
  const { data } = await api.put(`/api/alerts/${alertId}/acknowledge`);
  return data;
}

export async function scanItem(imageBase64: string) {
  const { data } = await api.post('/api/scan', { image: imageBase64 });
  return data;
}