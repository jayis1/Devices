import axios from 'axios';

const BASE = 'https://api.oralsync.cloud/v1';
let token: string | null = null;

export function setJwt(jwt: string) { token = jwt; }

const api = axios.create({ baseURL: BASE, timeout: 10000 });
api.interceptors.request.use(cfg => {
  if (token) cfg.headers.Authorization = `Bearer ${token}`;
  return cfg;
});

// ── Users ──
export const listUsers = () => api.get('/homes/me/users').then(r => r.data);
export const createUser = (name: string, age: number, orthodontic: boolean) =>
  api.post('/homes/me/users', { name, age, orthodontic }).then(r => r.data);

// ── Sessions ──
export const listSessions = (userId: number, limit = 50) =>
  api.get(`/users/${userId}/sessions`, { params: { limit } }).then(r => r.data);
export const uplinkSession = (s: any) => api.post('/sessions', s).then(r => r.data);

// ── Scans ──
export const listScans = (userId: number, limit = 50) =>
  api.get(`/users/${userId}/scans`, { params: { limit } }).then(r => r.data);

// ── Saliva ──
export const listSaliva = (userId: number, limit = 100) =>
  api.get(`/users/${userId}/saliva`, { params: { limit } }).then(r => r.data);

// ── Risk ──
export const getRisk = (userId: number, horizon = 90) =>
  api.get(`/users/${userId}/risk`, { params: { horizon_days: horizon } }).then(r => r.data);

// ── Reports ──
export const generateReport = (userId: number) =>
  api.post(`/users/${userId}/report`).then(r => r.data);

// ── WebSocket ──
export function openStream(onMsg: (evt: any) => void): WebSocket {
  const ws = new WebSocket(`wss://api.oralsync.cloud/v1/stream?jwt=${token}`);
  ws.onmessage = (e) => { try { onMsg(JSON.parse(e.data)); } catch {} };
  return ws;
}